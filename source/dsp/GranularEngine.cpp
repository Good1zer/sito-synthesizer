#include "GranularEngine.h"
#include "DSPUtils.h"

#include <cmath>

void GranularEngine::rebuildEnvelopeTables()
{
    // === ENVELOPE TABLE GENERATION ===
    // Pre-compute 4 grain-safe windows derived from shared shape family:
    // sine <> saw <> triangle <> square. Each one gets an edge fade so grains
    // still begin/end at zero and stay click-safe.
    
    for (int i = 0; i < envelopeTableSize; ++i)
    {
        const auto idx = static_cast<size_t> (i);
        const auto phase = static_cast<float> (i) / static_cast<float> (envelopeTableSize - 1);
        envelopeTables[0][idx] = SitoDSP::evaluateGrainShapeWindow (phase, 0.0f);
        envelopeTables[1][idx] = SitoDSP::evaluateGrainShapeWindow (phase, 1.0f);
        envelopeTables[2][idx] = SitoDSP::evaluateGrainShapeWindow (phase, 2.0f);
        envelopeTables[3][idx] = SitoDSP::evaluateGrainShapeWindow (phase, 3.0f);
    }
}

int GranularEngine::countActiveGrains (const std::array<Voice, maxVoices>& v)
{
    int count = 0;
    for (const auto& voice : v)
        for (const auto& grain : voice.grains)
            if (grain.isActive)
                ++count;
    return count;
}

int GranularEngine::copyGrainVisuals (GrainVisual* dest, int maxToCopy) const noexcept
{
    if (dest == nullptr || maxToCopy <= 0)
        return 0;

    const auto count = juce::jlimit (0, maxGrainVisuals, grainVisualCount.load (std::memory_order_acquire));
    const auto toCopy = juce::jmin (count, maxToCopy);

    for (int i = 0; i < toCopy; ++i)
        dest[i] = grainVisuals[static_cast<size_t> (i)];

    return toCopy;
}

void GranularEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    for (auto& scratch : voiceEnvelopeScratch)
        scratch.assign (static_cast<size_t> (juce::jmax (1, currentBlockSize)), 0.0f);
    rebuildEnvelopeTables();
    reset();
}

void GranularEngine::reset()
{
    for (auto& voice : voices)
        voice = {};

    smoothedGainDb = 0.0f;
    smoothedPosition = 0.5f;
    smoothedSpray = 0.0f;
    smoothedSpread = 0.0f;
    smoothedGrainSizeMs = 120.0f;
    smoothedDensityHz = 8.0f;
    smoothedPitchSemitones = 0.0f;
    smoothedShapeMorph = 1.0f;
    smoothedEnvAttackMs = 12.0f;
    smoothedEnvHoldMs = 0.0f;
    smoothedEnvDecayMs = 220.0f;
    smoothedEnvSustain = 0.82f;
    smoothedEnvReleaseMs = 260.0f;
}

void GranularEngine::setMaxVoices (int newMax) noexcept
{
    voiceLimit = juce::jlimit (1, maxVoicesSupported, newMax);
}

void GranularEngine::handleMidiEvent (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        // === VOICE ALLOCATION ===
        // Try to find a free voice first
        Voice* target = nullptr;

        for (int i = 0; i < voiceLimit; ++i)
        {
            auto& voice = voices[static_cast<size_t> (i)];
            if (! voice.isActive)
            {
                target = &voice;
                break;
            }
        }

        // === VOICE STEALING ===
        // No free voices? Steal the oldest one (LRU - Least Recently Used)
        // This prevents stuck notes and ensures polyphony stays within limits
        if (target == nullptr)
        {
            target = &voices[0];
            for (int i = 0; i < voiceLimit; ++i)
            {
                auto& voice = voices[static_cast<size_t> (i)];
                if (voice.startStamp < target->startStamp)
                    target = &voice;
            }
        }

        // === INITIALIZE NEW VOICE ===
        *target = {};
        target->isActive = true;
        target->isSpawning = true;
        target->noteHeld = true;
        target->midiNote = message.getNoteNumber();
        target->velocity = message.getFloatVelocity();
        target->grainSpawnAccumulator = 1.0;  // Spawn first grain immediately
        target->startStamp = ++voiceStampCounter;
        target->envelopeStage = EnvelopeStage::attack;
        target->envelopeLevel = 0.0f;
    }
    else if (message.isNoteOff())
    {
        for (int i = 0; i < voiceLimit; ++i)
        {
            auto& voice = voices[static_cast<size_t> (i)];
            if (voice.isActive && voice.midiNote == message.getNoteNumber())
            {
                voice.noteHeld = false;
                startVoiceRelease (voice);
            }
        }
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        for (int i = 0; i < voiceLimit; ++i)
        {
            auto& voice = voices[static_cast<size_t> (i)];
            if (voice.isActive)
            {
                voice.noteHeld = false;
                startVoiceRelease (voice);
            }
        }
    }
}

void GranularEngine::render (juce::AudioBuffer<float>& outputBuffer,
                             const SampleData& sampleData,
                             float gainDb,
                             float positionNormalized,
                             float sprayNormalized,
                             float spreadNormalized,
                             float grainSizeMs,
                             float densityHz,
                             float pitchSemitones,
                             float shapeMorph,
                             float envAttackMs,
                             float envHoldMs,
                             float envDecayMs,
                             float envSustain,
                             float envReleaseMs,
                             bool softClipEnabled,
                             bool trueStereoEnabled,
                             bool useCubicInterpolation)
{
    // Safety: ensure we have valid sample rate before processing
    jassert (currentSampleRate > 0.0);
    if (currentSampleRate <= 0.0)
        return;

    sampleData.withTryReadAccess ([&] (const juce::AudioBuffer<float>& sampleBuffer, double loadedSampleRate)
    {
        const auto numSourceChannels = sampleBuffer.getNumChannels();
        const auto numSourceSamples = sampleBuffer.getNumSamples();

        if (numSourceChannels <= 0 || numSourceSamples < 2)
            return;

        // === PARAMETER VALIDATION ===
        // Density is "grains per second". We hard-cap the effective spawn rate to prevent
        // pathological CPU spikes when the user cranks Density (common cause of crackles).
        constexpr float maxEffectiveDensity = 1200.0f;
        
        // Validate sample data is usable
        jassert (numSourceChannels > 0 && numSourceSamples >= 2);
        const auto targetDensity = juce::jlimit (0.1f, maxEffectiveDensity, densityHz);
        const auto targetPosition = juce::jlimit (0.0f, 1.0f, positionNormalized);
        const auto targetSpray = juce::jlimit (0.0f, 1.0f, sprayNormalized);
        const auto targetSpread = juce::jlimit (0.0f, 1.0f, spreadNormalized);
        const auto targetGrainSizeMs = juce::jlimit (5.0f, 750.0f, grainSizeMs);
        const auto targetShapeMorph = juce::jlimit (0.0f, 3.0f, shapeMorph);

        const auto outChannels = outputBuffer.getNumChannels();
        const auto outSamples = outputBuffer.getNumSamples();
        const auto blockSeconds = static_cast<double> (outSamples) / currentSampleRate;

        // Smooth at block-rate to avoid zipper noise and sudden CPU bursts.
        smoothedGainDb = SitoDSP::smoothParam (smoothedGainDb, gainDb, blockSeconds, 0.040);
        smoothedPosition = SitoDSP::smoothParam (smoothedPosition, targetPosition, blockSeconds, 0.030);
        smoothedSpray = SitoDSP::smoothParam (smoothedSpray, targetSpray, blockSeconds, 0.035);
        smoothedSpread = SitoDSP::smoothParam (smoothedSpread, targetSpread, blockSeconds, 0.050);
        smoothedGrainSizeMs = SitoDSP::smoothParam (smoothedGrainSizeMs, targetGrainSizeMs, blockSeconds, 0.060);
        smoothedDensityHz = SitoDSP::smoothParam (smoothedDensityHz, targetDensity, blockSeconds, 0.060);
        smoothedPitchSemitones = SitoDSP::smoothParam (smoothedPitchSemitones, pitchSemitones, blockSeconds, 0.050);
        smoothedShapeMorph = SitoDSP::smoothParam (smoothedShapeMorph, targetShapeMorph, blockSeconds, 0.050);
        smoothedEnvAttackMs = SitoDSP::smoothParam (smoothedEnvAttackMs, juce::jmax (0.0f, envAttackMs), blockSeconds, 0.050);
        smoothedEnvHoldMs = SitoDSP::smoothParam (smoothedEnvHoldMs, juce::jmax (0.0f, envHoldMs), blockSeconds, 0.050);
        smoothedEnvDecayMs = SitoDSP::smoothParam (smoothedEnvDecayMs, juce::jmax (0.0f, envDecayMs), blockSeconds, 0.050);
        smoothedEnvSustain = SitoDSP::smoothParam (smoothedEnvSustain, juce::jlimit (0.0f, 1.0f, envSustain), blockSeconds, 0.050);
        smoothedEnvReleaseMs = SitoDSP::smoothParam (smoothedEnvReleaseMs, juce::jmax (0.0f, envReleaseMs), blockSeconds, 0.050);

        const auto clampedDensity = juce::jlimit (0.1f, maxEffectiveDensity, smoothedDensityHz);
        const auto clampedPosition = juce::jlimit (0.0f, 1.0f, smoothedPosition);
        const auto clampedSpray = juce::jlimit (0.0f, 1.0f, smoothedSpray);
        const auto clampedSpread = juce::jlimit (0.0f, 1.0f, smoothedSpread);
        const auto clampedGrainSizeMs = juce::jmax (5.0f, smoothedGrainSizeMs);
        const auto clampedShapeMorph = juce::jlimit (0.0f, 3.0f, smoothedShapeMorph);
        const auto clampedEnvAttackMs = juce::jmax (0.0f, smoothedEnvAttackMs);
        const auto clampedEnvHoldMs = juce::jmax (0.0f, smoothedEnvHoldMs);
        const auto clampedEnvDecayMs = juce::jmax (0.0f, smoothedEnvDecayMs);
        const auto clampedEnvSustain = juce::jlimit (0.0f, 1.0f, smoothedEnvSustain);
        const auto clampedEnvReleaseMs = juce::jmax (0.0f, smoothedEnvReleaseMs);

        // === GAIN COMPENSATION ===
        // Output gain with perceptual loudness compensation for stacked grains.
        // Using sqrt prevents excessive gain reduction when many grains overlap.
        // Formula: gain = 1 / sqrt(1 + 0.55 * grainCount)
        // This keeps perceived loudness stable as density increases.
        const auto activeGrains = countActiveGrains (voices);
        const auto stackComp = 1.0f / std::sqrt (1.0f + 0.55f * static_cast<float> (activeGrains));
        const auto outputGain = juce::Decibels::decibelsToGain (smoothedGainDb) * stackComp;

        // Shape is constant for this render call.
        const auto shapeMorphConst = clampedShapeMorph;

        // Destination pointers once per block.
        auto* outL = outChannels > 0 ? outputBuffer.getWritePointer (0) : nullptr;
        auto* outR = outChannels > 1 ? outputBuffer.getWritePointer (1) : nullptr;

        // Source pointers once per block (huge win vs calling getReadPointer in the inner loop).
        const auto* sourceDataL = sampleBuffer.getReadPointer (0);
        const auto* sourceDataR = sampleBuffer.getReadPointer (juce::jmin (1, numSourceChannels - 1));
        const auto isMonoOutput = (outChannels <= 1);
        const auto useTrueStereo = trueStereoEnabled && numSourceChannels > 1 && outChannels > 1;

        // If the user reduces max voices while notes are held, gracefully stop voices beyond the limit.
        for (int i = voiceLimit; i < static_cast<int> (maxVoices); ++i)
        {
            auto& voice = voices[static_cast<size_t> (i)];
            if (! voice.isActive)
                continue;

            voice.isSpawning = false;
            if (! voiceHasActiveGrains (voice))
                voice = {};
        }

        // === GRAIN SPAWNING ===
        // Spawn grains per-block, not per-sample (big perf win).
        // Budget limits total grains spawned per block to prevent CPU explosions.
        // Formula: density * blockTime + safety margin, capped at 64 grains/block.
        int globalSpawnBudget = juce::jlimit (4, 64, static_cast<int> (std::ceil (clampedDensity * static_cast<float> (blockSeconds))) + 4);
        jassert (globalSpawnBudget > 0 && globalSpawnBudget <= 64);

        for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
        {
            auto& voice = voices[static_cast<size_t> (voiceIndex)];
            if (! voice.isActive || ! voice.isSpawning)
                continue;

            voice.grainSpawnAccumulator += static_cast<double> (clampedDensity) * blockSeconds;

            auto toSpawn = static_cast<int> (std::floor (voice.grainSpawnAccumulator));
            if (toSpawn <= 0)
                continue;

            // Per-voice cap keeps polyphony from exploding CPU.
            toSpawn = juce::jmin (toSpawn, 12);
            toSpawn = juce::jmin (toSpawn, globalSpawnBudget);

            for (int i = 0; i < toSpawn; ++i)
            {
                spawnGrain (voice,
                            sampleBuffer,
                            loadedSampleRate,
                            clampedPosition,
                            clampedSpray,
                            clampedSpread,
                            clampedGrainSizeMs,
                            smoothedPitchSemitones);
            }

            voice.grainSpawnAccumulator -= static_cast<double> (toSpawn);
            globalSpawnBudget -= toSpawn;
            if (globalSpawnBudget <= 0)
                break;
        }

        // === GRAIN RENDERING ===
        // Render all active grains to output buffer.
        // Each grain reads from sample with linear interpolation and applies envelope.
        for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
        {
            auto& voice = voices[static_cast<size_t> (voiceIndex)];
            if (! voice.isActive)
                continue;
            
            auto& voiceEnvelope = voiceEnvelopeScratch[static_cast<size_t> (voiceIndex)];
            if (static_cast<int> (voiceEnvelope.size()) < outSamples)
                voiceEnvelope.resize (static_cast<size_t> (outSamples), 0.0f);

            for (int sample = 0; sample < outSamples; ++sample)
            {
                voiceEnvelope[static_cast<size_t> (sample)] = advanceVoiceEnvelope (voice,
                                                                                     1,
                                                                                     clampedEnvAttackMs,
                                                                                     clampedEnvHoldMs,
                                                                                     clampedEnvDecayMs,
                                                                                     clampedEnvSustain,
                                                                                     clampedEnvReleaseMs);
            }

            for (auto& grain : voice.grains)
            {
                if (! grain.isActive)
                    continue;

                // Pull constants out of the sample loop.
                const auto grainBaseGain = outputGain * grain.amplitude;
                const auto monoPan = 1.0f; // mono output should not lose level due to panning law

                const auto remainingSamples = grain.durationSamples - grain.ageSamples;
                if (remainingSamples <= 0)
                {
                    grain = {};
                    continue;
                }

                const auto samplesToProcess = juce::jmin (outSamples, remainingSamples);

                for (int sample = 0; sample < samplesToProcess; ++sample)
                {
                    // Get integer sample index and fractional part for interpolation
                    const auto index = static_cast<int> (grain.sourceSamplePosition);

                    // Safety: stop grain if it reads beyond sample bounds
                    // Cubic needs 4 points, so check bounds accordingly
                    const auto minIndex = useCubicInterpolation ? 1 : 0;
                    const auto maxIndex = useCubicInterpolation ? numSourceSamples - 3 : numSourceSamples - 1;
                    
                    if (index < minIndex || index >= maxIndex)
                    {
                        grain = {};
                        break;
                    }

                    // Fractional position between samples for interpolation
                    const auto alpha = static_cast<float> (grain.sourceSamplePosition - static_cast<double> (index));
                    jassert (alpha >= 0.0f && alpha <= 1.0f);
                    
                    const auto voiceEnvelopeValue = voiceEnvelope[static_cast<size_t> (sample)];

                    // Apply grain envelope (window function) to avoid clicks
                    const auto grainEnvelope = getEnvelopeValue (grain, shapeMorphConst);
                    const auto grainGain = grainBaseGain * grainEnvelope * voiceEnvelopeValue;

                    if (isMonoOutput)
                    {
                        float interpolatedSample;
                        
                        if (useCubicInterpolation)
                        {
                            // Cubic interpolation: 4-point for better quality
                            interpolatedSample = SitoDSP::cubicInterpolate (
                                sourceDataL[index - 1],
                                sourceDataL[index],
                                sourceDataL[index + 1],
                                sourceDataL[index + 2],
                                alpha
                            );
                        }
                        else
                        {
                            // Linear interpolation: fast and efficient
                            const auto currentSampleValue = sourceDataL[index];
                            const auto nextSampleValue = sourceDataL[index + 1];
                            interpolatedSample = currentSampleValue + (nextSampleValue - currentSampleValue) * alpha;
                        }

                        if (outL != nullptr)
                            outL[sample] += interpolatedSample * grainGain * monoPan;
                    }
                    else
                    {
                        float sampleL, sampleR;
                        
                        if (useCubicInterpolation)
                        {
                            // Cubic interpolation for both channels
                            sampleL = SitoDSP::cubicInterpolate (
                                sourceDataL[index - 1],
                                sourceDataL[index],
                                sourceDataL[index + 1],
                                sourceDataL[index + 2],
                                alpha
                            );
                            sampleR = SitoDSP::cubicInterpolate (
                                sourceDataR[index - 1],
                                sourceDataR[index],
                                sourceDataR[index + 1],
                                sourceDataR[index + 2],
                                alpha
                            );
                        }
                        else
                        {
                            // Linear interpolation for both channels
                            const auto curL = sourceDataL[index];
                            const auto nxtL = sourceDataL[index + 1];
                            const auto curR = sourceDataR[index];
                            const auto nxtR = sourceDataR[index + 1];
                            
                            sampleL = curL + (nxtL - curL) * alpha;
                            sampleR = curR + (nxtR - curR) * alpha;
                        }

                        if (useTrueStereo)
                        {
                            if (outL != nullptr) outL[sample] += sampleL * grainGain * grain.panL;
                            if (outR != nullptr) outR[sample] += sampleR * grainGain * grain.panR;
                        }
                        else
                        {
                            const auto mono = (sampleL + sampleR) * 0.5f;
                            if (outL != nullptr) outL[sample] += mono * grainGain * grain.panL;
                            if (outR != nullptr) outR[sample] += mono * grainGain * grain.panR;
                        }
                    }

                    grain.sourceSamplePosition += grain.sourceIncrement;
                    ++grain.ageSamples;
                }

                if (grain.ageSamples >= grain.durationSamples)
                    grain = {};
            }
        }

        for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
        {
            auto& voice = voices[static_cast<size_t> (voiceIndex)];
            if (voice.isActive && ! voice.noteHeld && voice.envelopeLevel <= 0.0f)
                voice.isSpawning = false;

            if (voice.isActive && ! voice.isSpawning && ! voiceHasActiveGrains (voice))
                voice = {};
        }

        // === GRAIN VISUALIZATION ===
        // UI snapshot: publish active grains (position/strength/pan) for waveform overlays.
        // Lock-free: UI reads this data via atomic counter.
        {
            int writeIndex = 0;
            const auto denom = static_cast<float> (juce::jmax (1, numSourceSamples - 1));

            for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
            {
                const auto& voice = voices[static_cast<size_t> (voiceIndex)];
                if (! voice.isActive)
                    continue;

                for (const auto& grain : voice.grains)
                {
                    if (! grain.isActive)
                        continue;

                    if (writeIndex >= maxGrainVisuals)
                        break;

                    const auto pos = static_cast<float> (grain.sourceSamplePosition) / denom;
                    const auto env = getEnvelopeValue (grain, shapeMorphConst);
                    const auto strength = juce::jlimit (0.0f, 1.0f, env * grain.amplitude * voice.envelopeLevel);
                    const auto pan = juce::jlimit (-1.0f, 1.0f, grain.panR - grain.panL);

                    grainVisuals[static_cast<size_t> (writeIndex)] = { juce::jlimit (0.0f, 1.0f, pos), strength, pan };
                    ++writeIndex;
                }

                if (writeIndex >= maxGrainVisuals)
                    break;
            }

            grainVisualCount.store (writeIndex, std::memory_order_release);
        }

        // === OUTPUT PROTECTION ===
        // Final safety: optional soft-clip to avoid harsh digital clipping.
        // Keeps output in safe range when many grains stack unexpectedly.
        if (softClipEnabled)
        {
            for (int ch = 0; ch < outChannels; ++ch)
            {
                auto* data = outputBuffer.getWritePointer (ch);
                if (data == nullptr)
                    continue;

                for (int i = 0; i < outSamples; ++i)
                    data[i] = SitoDSP::softClip (data[i]);
            }
        }
    });
}

void GranularEngine::spawnGrain (Voice& voice,
                                 const juce::AudioBuffer<float>& sampleBuffer,
                                 double loadedSampleRate,
                                 float positionNormalized,
                                 float sprayNormalized,
                                 float spreadNormalized,
                                 float grainSizeMs,
                                 float pitchSemitones)
{
    // === FIND FREE GRAIN SLOT ===
    // Each voice has 24 grain slots. Find first inactive one.
    Grain* freeGrain = nullptr;

    for (auto& grain : voice.grains)
    {
        if (! grain.isActive)
        {
            freeGrain = &grain;
            break;
        }
    }

    // Safety: validate we have resources and valid sample rates
    jassert (loadedSampleRate > 0.0 && currentSampleRate > 0.0);
    if (freeGrain == nullptr || loadedSampleRate <= 0.0 || currentSampleRate <= 0.0)
        return;

    // === CALCULATE GRAIN START POSITION ===
    // Position: where in sample to read (0..1)
    // Spray: random variation around position (creates texture)
    const auto sprayHalfWidth = sprayNormalized * 0.5f;
    const auto jitter = rng.nextFloat() * 2.0f - 1.0f;  // Random value in [-1, 1]
    const auto jitteredPosition = juce::jlimit (0.0f, 1.0f, positionNormalized + jitter * sprayHalfWidth);
    const auto numSamples = sampleBuffer.getNumSamples();
    const auto maxStartSample = juce::jmax (1, numSamples - 2);
    const auto startSample = static_cast<double> (maxStartSample) * jitteredPosition;
    
    // === CALCULATE GRAIN DURATION ===
    // Convert milliseconds to samples at output sample rate
    const auto durationSeconds = static_cast<double> (grainSizeMs) * 0.001;
    const auto durationSamples = juce::jmax (8, static_cast<int> (durationSeconds * currentSampleRate));
    
    // === CALCULATE PITCH SHIFT ===
    // Pitch is controlled by playback speed (sourceIncrement)
    // MIDI note tracking: C4 (60) = no shift, each semitone = 2^(1/12) ratio
    const auto pitchOffsetInSemitones = pitchSemitones + static_cast<float> (voice.midiNote - 60);
    const auto pitchRatio = std::pow (2.0, static_cast<double> (pitchOffsetInSemitones) / 12.0);
    
    // === CALCULATE STEREO PANNING ===
    // Spread: random pan position for each grain (creates stereo width)
    // Using constant-power panning law (sin/cos) for smooth stereo image
    const auto pan = (rng.nextFloat() * 2.0f - 1.0f) * spreadNormalized;
    const auto panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

    // === INITIALIZE GRAIN ===
    freeGrain->isActive = true;
    freeGrain->sourceSamplePosition = startSample;
    freeGrain->sourceIncrement = (loadedSampleRate / currentSampleRate) * pitchRatio;
    freeGrain->ageSamples = 0;
    freeGrain->durationSamples = durationSamples;
    freeGrain->amplitude = voice.velocity;
    freeGrain->panL = std::cos (panAngle);
    freeGrain->panR = std::sin (panAngle);
    
    // Validate grain parameters in debug builds
    jassert (freeGrain->durationSamples > 0);
    jassert (freeGrain->sourceIncrement > 0.0);
    jassert (freeGrain->amplitude >= 0.0f && freeGrain->amplitude <= 1.0f);
}

float GranularEngine::getEnvelopeValue (const Grain& grain, float shapeMorph) const
{
    if (grain.durationSamples <= 1)
        return 0.0f;

    const auto phase = static_cast<float> (grain.ageSamples) / static_cast<float> (grain.durationSamples - 1);

    const auto leftIndex = juce::jlimit (0, 3, static_cast<int> (std::floor (shapeMorph)));
    const auto rightIndex = juce::jlimit (0, 3, leftIndex + 1);
    const auto blend = shapeMorph - static_cast<float> (leftIndex);

    auto evalTable = [&] (int type) -> float
    {
        const auto t = juce::jlimit (0.0f, 1.0f, phase) * static_cast<float> (envelopeTableSize - 1);
        const auto index = static_cast<int> (t);
        const auto frac = t - static_cast<float> (index);
        const auto i0 = static_cast<size_t> (juce::jlimit (0, envelopeTableSize - 1, index));
        const auto i1 = static_cast<size_t> (juce::jlimit (0, envelopeTableSize - 1, index + 1));
        const auto typeIdx = static_cast<size_t> (type);
        const auto a = envelopeTables[typeIdx][i0];
        const auto b = envelopeTables[typeIdx][i1];
        return a + (b - a) * frac;
    };

    const auto a = evalTable (leftIndex);
    const auto b = evalTable (rightIndex);
    return a + (b - a) * blend;
}

bool GranularEngine::voiceHasActiveGrains (const Voice& voice) const
{
    for (const auto& grain : voice.grains)
        if (grain.isActive)
            return true;
    return false;
}

float GranularEngine::advanceVoiceEnvelope (Voice& voice,
                                            int samplesToAdvance,
                                            float attackMs,
                                            float holdMs,
                                            float decayMs,
                                            float sustainLevel,
                                            float releaseMs) noexcept
{
    if (! voice.isActive)
        return 0.0f;

    const auto samplesPerMs = static_cast<float> (currentSampleRate / 1000.0);

    for (int step = 0; step < samplesToAdvance; ++step)
    {
        switch (voice.envelopeStage)
        {
            case EnvelopeStage::idle:
                voice.envelopeLevel = 0.0f;
                break;

            case EnvelopeStage::attack:
            {
                const auto attackSamples = juce::jmax (1, static_cast<int> (std::round (attackMs * samplesPerMs)));
                voice.envelopeLevel += 1.0f / static_cast<float> (attackSamples);
                if (voice.envelopeLevel >= 1.0f)
                {
                    voice.envelopeLevel = 1.0f;
                    voice.envelopeSamplesRemaining = juce::jmax (0, static_cast<int> (std::round (holdMs * samplesPerMs)));
                    voice.envelopeStage = voice.envelopeSamplesRemaining > 0 ? EnvelopeStage::hold : EnvelopeStage::decay;
                }
                break;
            }

            case EnvelopeStage::hold:
                if (--voice.envelopeSamplesRemaining <= 0)
                    voice.envelopeStage = EnvelopeStage::decay;
                break;

            case EnvelopeStage::decay:
            {
                const auto decaySamples = juce::jmax (1, static_cast<int> (std::round (decayMs * samplesPerMs)));
                voice.envelopeLevel += (sustainLevel - 1.0f) / static_cast<float> (decaySamples);
                if (voice.envelopeLevel <= sustainLevel)
                {
                    voice.envelopeLevel = sustainLevel;
                    voice.envelopeStage = EnvelopeStage::sustain;
                }
                break;
            }

            case EnvelopeStage::sustain:
                voice.envelopeLevel = sustainLevel;
                if (! voice.noteHeld)
                    startVoiceRelease (voice);
                break;

            case EnvelopeStage::release:
            {
                const auto releaseSamples = juce::jmax (1, static_cast<int> (std::round (releaseMs * samplesPerMs)));
                voice.envelopeLevel -= voice.releaseStartLevel / static_cast<float> (releaseSamples);
                if (voice.envelopeLevel <= 0.0f)
                {
                    voice.envelopeLevel = 0.0f;
                    voice.envelopeStage = EnvelopeStage::idle;
                }
                break;
            }
        }
    }

    return juce::jlimit (0.0f, 1.0f, voice.envelopeLevel);
}

void GranularEngine::startVoiceRelease (Voice& voice) noexcept
{
    if (! voice.isActive || voice.envelopeStage == EnvelopeStage::idle || voice.envelopeStage == EnvelopeStage::release)
        return;

    voice.isSpawning = true;
    voice.envelopeStage = EnvelopeStage::release;
    voice.releaseStartLevel = juce::jmax (0.0f, voice.envelopeLevel);
}
