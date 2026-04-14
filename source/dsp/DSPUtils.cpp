#include "DSPUtils.h"
#include <juce_core/juce_core.h>

namespace SitoDSP
{

float softClip (float x) noexcept
{
    // Fast, smooth saturation (no branches). Keeps levels sane when many grains stack.
    // y = x / (1 + |x|) maps to (-1..1), preserving small-signal detail.
    const auto a = std::abs (x);
    return x / (1.0f + a);
}

float smoothParam (float current, float target, double blockSeconds, double tauSeconds) noexcept
{
    if (tauSeconds <= 0.0 || blockSeconds <= 0.0)
        return target;

    // One-pole smoothing at block rate. alpha ~= 1 - exp(-dt/tau).
    const auto alpha = 1.0f - std::exp (-blockSeconds / tauSeconds);
    return current + (target - current) * static_cast<float> (juce::jlimit (0.0, 1.0, alpha));
}

float cubicInterpolate (float y0, float y1, float y2, float y3, float mu) noexcept
{
    // === CUBIC INTERPOLATION (4-point, 3rd order Hermite) ===
    // Higher quality than linear, better for pitched-down grains
    // y0, y1, y2, y3 are four consecutive samples
    // mu is fractional position between y1 and y2 (0..1)
    // Formula: Catmull-Rom spline for smooth interpolation
    
    const auto mu2 = mu * mu;
    const auto a0 = y3 - y2 - y0 + y1;
    const auto a1 = y0 - y1 - a0;
    const auto a2 = y2 - y0;
    const auto a3 = y1;
    
    return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}

} // namespace SitoDSP
