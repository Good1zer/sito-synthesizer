#pragma once

#include <cmath>

namespace SitoDSP
{
    // ============================================================================
    // PURE DSP UTILITY FUNCTIONS
    // ============================================================================
    // These are stateless, pure functions that can be tested independently
    // and reused across different parts of the audio engine.
    
    /**
     * Soft clipping/saturation function using tanh-like curve.
     * Maps input to (-1, 1) range with smooth saturation.
     * Formula: y = x / (1 + |x|)
     * 
     * @param x Input signal (unbounded)
     * @return Soft-clipped output in range (-1, 1)
     */
    float softClip (float x) noexcept;
    
    /**
     * One-pole smoothing filter for parameter changes.
     * Prevents zipper noise and CPU spikes from sudden parameter changes.
     * 
     * @param current Current smoothed value
     * @param target Target value to reach
     * @param blockSeconds Duration of current audio block in seconds
     * @param tauSeconds Time constant (63% settling time)
     * @return New smoothed value
     */
    float smoothParam (float current, float target, double blockSeconds, double tauSeconds) noexcept;
    
    /**
     * Cubic (Hermite) interpolation for high-quality sample playback.
     * Uses 4-point, 3rd order Catmull-Rom spline.
     * 
     * @param y0 Sample at position -1
     * @param y1 Sample at position 0 (interpolating from here)
     * @param y2 Sample at position 1 (interpolating to here)
     * @param y3 Sample at position 2
     * @param mu Fractional position between y1 and y2 (0..1)
     * @return Interpolated sample value
     */
    float cubicInterpolate (float y0, float y1, float y2, float y3, float mu) noexcept;
    
    /**
     * Linear interpolation between two samples.
     * Faster than cubic but lower quality for pitched playback.
     * 
     * @param y1 First sample
     * @param y2 Second sample
     * @param mu Fractional position between y1 and y2 (0..1)
     * @return Interpolated sample value
     */
    inline float linearInterpolate (float y1, float y2, float mu) noexcept
    {
        return y1 + (y2 - y1) * mu;
    }
    
    /**
     * Convert semitones to frequency ratio.
     * Used for pitch shifting calculations.
     * 
     * @param semitones Pitch shift in semitones (12 = one octave)
     * @return Frequency multiplication ratio
     */
    inline float semitonesToRatio (float semitones) noexcept
    {
        return std::pow (2.0f, semitones / 12.0f);
    }
    
} // namespace SitoDSP
