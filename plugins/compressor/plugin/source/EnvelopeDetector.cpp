//
//  EnvelopeDetector.cpp
//  Compressor - Shared Code
//
//  Created by Mark Daunt on 28/1/18.
//
//  See EnvelopeDetector.h for the derivation of the coefficient formula and
//  the design discussion; the comments here cover implementation details.
//

#include "EnvelopeDetector.h"
#include <cmath>

void EnvelopeDetector::prepare (float sampleRateIn)
{
    sampleRate = sampleRateIn;
    updateCoefficients();
    reset();
}

void EnvelopeDetector::reset()
{
    envelope = 0.0f;
}

void EnvelopeDetector::setAttackTime (float attackMsIn)
{
    attackMs = attackMsIn;
    updateCoefficients();
}

void EnvelopeDetector::setReleaseTime (float releaseMsIn)
{
    releaseMs = releaseMsIn;
    updateCoefficients();
}

void EnvelopeDetector::setTimeConstant (TimeConstant conventionIn)
{
    timeConstant = conventionIn;
    updateCoefficients();
}

void EnvelopeDetector::updateCoefficients()
{
    // a = exp( ln(eps) / N ), N = samples in the requested time.
    // (Derived in the header: solve a^N = eps for a.)
    const float tc = (timeConstant == TimeConstant::Analog) ? kAnalogTC : kDigitalTC;

    attackCoeff  = std::exp (tc / (attackMs  * sampleRate * 0.001f));
    releaseCoeff = std::exp (tc / (releaseMs * sampleRate * 0.001f));
}

float EnvelopeDetector::processSample (float x)
{
    // ------------------------------------------------------------------
    // 1) Rectify: reduce the bipolar waveform to a non-negative statistic.
    //    Peak mode tracks |x| (amplitude domain); RMS mode tracks x^2
    //    (power domain - the Mean and Root come later, in that order).
    // ------------------------------------------------------------------
    const float rectified = (mode == Mode::RMS) ? x * x : std::fabs (x);

    // ------------------------------------------------------------------
    // 2) Smooth with the branching one-pole:
    //        y[n] = a * y[n-1] + (1 - a) * in
    //    written in the equivalent "lerp toward the input" form. Using the
    //    attack coefficient while the envelope is rising and the release
    //    coefficient while falling is the asymmetry that turns a plain
    //    lowpass filter into an envelope follower.
    //
    //    Note for RMS mode: the smoothing acts on the *power* signal, so a
    //    given coefficient moves the level (in dB) roughly twice as fast as
    //    the same coefficient would in peak mode. This is inherent to
    //    power-domain averaging; the labeled times remain exact by their
    //    own definition (time to cover 99% of a step in the smoothed
    //    domain).
    // ------------------------------------------------------------------
    const float coeff = (rectified > envelope) ? attackCoeff : releaseCoeff;
    envelope = coeff * (envelope - rectified) + rectified;

    // Flush subnormals. Once the input goes silent the exponential decay
    // walks the envelope down into denormal territory, where floating-point
    // math can be dramatically slower on some CPUs. Well below any audible
    // level (1e-30 is about -600 dB), so just snap to zero.
    if (envelope < 1.0e-30f)
        envelope = 0.0f;

    // NOTE: deliberately NO upper clamp. The original code clamped the
    // envelope to 1.0 (0 dBFS), which blinded the detector exactly when the
    // signal ran hottest - float busses routinely exceed 0 dBFS, and a
    // compressor must keep responding up there.

    // ------------------------------------------------------------------
    // 3) Output in the amplitude domain. For RMS this is the "Root" step:
    //    sqrt of the running mean of the square. (The historic bug took the
    //    sqrt before smoothing, collapsing RMS into peak mode.)
    // ------------------------------------------------------------------
    return (mode == Mode::RMS) ? std::sqrt (envelope) : envelope;
}

//==============================================================================
void SidechainFilter::prepare (float sampleRateIn)
{
    sampleRate = sampleRateIn;
    setCutoff (cutoffHz);
    reset();
}

void SidechainFilter::reset()
{
    lp1 = 0.0f;
    lp2 = 0.0f;
}

void SidechainFilter::setCutoff (float cutoffHzIn)
{
    cutoffHz = cutoffHzIn;
    k = (cutoffHz > 0.0f)
            ? 1.0f - std::exp (-2.0f * 3.14159265f * cutoffHz / sampleRate)
            : 0.0f;
}

float SidechainFilter::processSample (float x)
{
    if (cutoffHz <= 0.0f)
        return x; // exact bypass - not "filter at 0 Hz", which would still add state lag

    lp1 += k * (x - lp1);
    const float hp1 = x - lp1;
    lp2 += k * (hp1 - lp2);
    return hp1 - lp2;
}

//==============================================================================
void DecoupledSmoother::prepare (float sampleRateIn)
{
    sampleRate = sampleRateIn;
    updateCoefficients();
    reset();
}

void DecoupledSmoother::reset()
{
    releaseState = 0.0f;
    smoothState = 0.0f;
}

void DecoupledSmoother::setAttackTime (float attackMsIn)
{
    attackMs = attackMsIn;
    updateCoefficients();
}

void DecoupledSmoother::setReleaseTime (float releaseMsIn)
{
    releaseMs = releaseMsIn;
    updateCoefficients();
}

void DecoupledSmoother::updateCoefficients()
{
    // Same derivation as EnvelopeDetector (see that header); the digital
    // 1%-remaining convention.
    attackCoeff  = std::exp (EnvelopeDetector::kDigitalTC / (attackMs  * sampleRate * 0.001f));
    releaseCoeff = std::exp (EnvelopeDetector::kDigitalTC / (releaseMs * sampleRate * 0.001f));
}

float DecoupledSmoother::processSample (float x)
{
    // Release stage: can jump up to the input instantly (the max), but can
    // only fall away from it at the release rate, releasing TOWARD the
    // input rather than toward zero - the "smooth" variant, so the
    // envelope keeps moving when the signal settles on a plateau.
    releaseState = std::fmax (x, releaseCoeff * releaseState + (1.0f - releaseCoeff) * x);

    // Attack stage: plain one-pole lowpass of the release trajectory.
    smoothState = attackCoeff * smoothState + (1.0f - attackCoeff) * releaseState;

    // Flush subnormals (same rationale as the level detector).
    if (smoothState < 1.0e-30f)
        smoothState = 0.0f;

    return smoothState;
}
