//
//  Compressor.cpp
//  Compressor - Shared Code
//
//  Created by Mark Daunt on 18/2/18.
//
//  See Compressor.h for the design discussion (feed-forward topology, the
//  Giannoulis/Massberg/Reiss quadratic knee, and why detection is
//  stereo-linked). References:
//
//    - W. Pirkle, "Designing Audio Effect Plugins in C++", ch. 13
//    - D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range
//      Compressor Design - A Tutorial and Analysis", JAES 60(6), 2012
//

#include "Compressor.h"
#include <algorithm>

Compressor::Compressor (int numChannelsIn)
    : detectors ((size_t) (numChannelsIn > 0 ? numChannelsIn : 1)),
      scFilters (detectors.size())
{
}

void Compressor::prepare (float sampleRate)
{
    for (auto& d : detectors)
    {
        d.prepare (sampleRate);
        d.setAttackTime (attackMs);
        d.setReleaseTime (releaseMs);
        d.setMode (detectorMode);
    }

    for (auto& f : scFilters)
        f.prepare (sampleRate);

    gainSmoother.prepare (sampleRate);
    gainSmoother.setAttackTime (attackMs);
    gainSmoother.setReleaseTime (releaseMs);
}

void Compressor::setSidechainCutoff (float cutoffHz)
{
    for (auto& f : scFilters)
        f.setCutoff (cutoffHz);
}

void Compressor::setAttack (float attackMsIn)
{
    attackMs = attackMsIn;
    for (auto& d : detectors)
        d.setAttackTime (attackMs);
    gainSmoother.setAttackTime (attackMs);
}

void Compressor::setRelease (float releaseMsIn)
{
    releaseMs = releaseMsIn;
    for (auto& d : detectors)
        d.setReleaseTime (releaseMs);
    gainSmoother.setReleaseTime (releaseMs);
}

void Compressor::setTopology (Topology topologyIn)
{
    if (topology == topologyIn)
        return;

    topology = topologyIn;
    resetBallistics();
}

void Compressor::setDetectorMode (EnvelopeDetector::Mode modeIn)
{
    if (detectorMode == modeIn)
        return;

    detectorMode = modeIn;
    for (auto& d : detectors)
        d.setMode (modeIn);
    resetBallistics();
}

void Compressor::resetBallistics()
{
    for (auto& d : detectors)
        d.reset();
    for (auto& f : scFilters)
        f.reset();
    gainSmoother.reset();
}

float Compressor::getAttack() const  { return attackMs; }
float Compressor::getRelease() const { return releaseMs; }

float Compressor::computeGainDb (float levelDb) const
{
    // Gain slope above the knee: 0 at 1:1 (no compression), -0.75 at 4:1,
    // approaching -1 as ratio -> infinity (limiting). Everything below is
    // Giannoulis et al. Eq. 4 rearranged as a gain (output minus input).
    const float slope = 1.0f / ratio - 1.0f;
    const float overshoot = levelDb - threshold; // how far past threshold, dB

    // Below the knee: unity gain. Written as 2*overshoot <= -W so the
    // hard-knee case (W = 0) falls cleanly into the first two branches and
    // the division by W below can never see zero.
    if (2.0f * overshoot <= -kneeWidth)
        return 0.0f;

    // Above the knee: the plain compression line, floored at the range
    // ceiling (see maxReductionDb in the header).
    if (2.0f * overshoot >= kneeWidth)
        return std::max (slope * overshoot, -maxReductionDb);

    // Inside the knee [T - W/2, T + W/2]: the quadratic blend. Matches the
    // outer segments in value and derivative at both edges (see header),
    // so the effective ratio glides from 1:1 to R with no corner.
    const float kneePos = overshoot + kneeWidth * 0.5f; // 0 .. W across the knee
    return std::max (slope * kneePos * kneePos / (2.0f * kneeWidth), -maxReductionDb);
}

float Compressor::processSampleLinked (const float* const* channelData, int numChannels, int sampleIndex)
{
    const size_t channelsToRead = std::min (detectors.size(), (size_t) (numChannels > 0 ? numChannels : 0));

    if (topology == Topology::LogDomain)
    {
        // Giannoulis et al. Fig. 7c / Eq. 23. Level sensing is
        // instantaneous - the stereo link is just the loudest rectified
        // sample - and the static curve runs on it immediately, so there
        // is no attack lag. ALL time behavior is then applied to the
        // gain-reduction signal, in dB, by the decoupled smoother.
        float peak = 0.0f;
        for (size_t ch = 0; ch < channelsToRead; ++ch)
        {
            // Sidechain HPF first: detection listens to the filtered copy,
            // the audio path never sees the filter.
            const float sideChain = scFilters[ch].processSample (channelData[ch][sampleIndex]);
            const float rectified = std::fabs (sideChain);
            if (rectified > peak)
                peak = rectified;
        }

        // computeGainDb is <= 0; the smoother wants the positive
        // "how many dB are we reducing" control signal (paper's x_L).
        const float grDb = -computeGainDb (amplitudeToDb (peak));
        const float smoothedGrDb = gainSmoother.processSample (grDb);

        return dbToAmplitude (-smoothedGrDb);
    }

    // Classic (Fig. 7a): smooth the level first, per channel. Run every
    // channel's detector every sample regardless of which one "wins" -
    // each envelope must track its own channel continuously or its
    // attack/release state would be corrupted whenever the other channel
    // takes over the link.
    float linkedLevel = 0.0f;

    for (size_t ch = 0; ch < channelsToRead; ++ch)
    {
        const float sideChain = scFilters[ch].processSample (channelData[ch][sampleIndex]);
        const float env = detectors[ch].processSample (sideChain);
        if (env > linkedLevel)
            linkedLevel = env;
    }

    // One conversion to dB, one trip through the static curve, one gain for
    // all channels: this single shared gain is what preserves the stereo
    // image (see header).
    const float gainDb = computeGainDb (amplitudeToDb (linkedLevel));

    return dbToAmplitude (gainDb);
}
