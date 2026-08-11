#include "BiquadDesign.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace eq
{

namespace
{
constexpr double kPi = 3.14159265358979323846;
} // namespace

BiquadCoefficients design (FilterType type, double freqHz, double gainDb,
                           double q, double sampleRate)
{
    // Clamp into the audio band; a centre frequency at/above Nyquist makes
    // the trig blow up, and a host can hand us any automation value.
    freqHz = std::clamp (freqHz, 10.0, 0.49 * sampleRate);
    q = std::clamp (q, 0.025, 40.0);

    const double w0 = 2.0 * kPi * freqHz / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);

    double b0, b1, b2, a0, a1, a2;

    switch (type)
    {
        case FilterType::bell:
            // Peaking EQ: poles and zeros share w0; A>1 pushes zeros out /
            // poles in (boost), A<1 the reverse (cut). Perfectly symmetric:
            // +12 dB then -12 dB at the same f0/Q is exact unity.
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha / A;
            break;

        case FilterType::lowShelf:
        {
            // Cookbook low shelf with slope S=1: alpha term becomes
            // sin(w0)/2 * sqrt((A + 1/A)(1/S - 1) + 2) = sin(w0)/2 * sqrt(2)
            // ... folded into the 2*sqrt(A)*alpha form below.
            const double sqrtA2alpha = 2.0 * std::sqrt (A) * (sinw0 / 2.0) * std::sqrt (2.0);
            b0 = A * ((A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 = (A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
            a2 = (A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha;
            break;
        }

        case FilterType::highShelf:
        {
            const double sqrtA2alpha = 2.0 * std::sqrt (A) * (sinw0 / 2.0) * std::sqrt (2.0);
            b0 = A * ((A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 = (A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
            a2 = (A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha;
            break;
        }

        case FilterType::highPass:
            // Zeros pinned at DC (z = 1, hence the (1+cosw0) numerator
            // shape): infinite rejection at 0 Hz, -3 dB at f0 for
            // Q = 1/sqrt2 (Butterworth), then +12 dB/oct.
            b0 = (1.0 + cosw0) / 2.0;
            b1 = -(1.0 + cosw0);
            b2 = (1.0 + cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::lowPass:
            b0 = (1.0 - cosw0) / 2.0;
            b1 = 1.0 - cosw0;
            b2 = (1.0 - cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::notch:
        default:
            // Zeros exactly ON the unit circle at w0: a true null. Width
            // comes from how tightly the poles hug the zeros (alpha/Q).
            b0 = 1.0;
            b1 = -2.0 * cosw0;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;
    }

    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

double magnitudeAt (const BiquadCoefficients& c, double freqHz, double sampleRate)
{
    // Evaluate H(z) on the unit circle at z = e^{jw}. Complex arithmetic is
    // the honest way - no approximation, and it's only called from the UI
    // curve and tests, never the audio thread.
    const double w = 2.0 * kPi * freqHz / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);      // z^-1
    const std::complex<double> z2 = std::polar (1.0, -2.0 * w); // z^-2

    const std::complex<double> numerator = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> denominator = 1.0 + c.a1 * z1 + c.a2 * z2;

    return std::abs (numerator / denominator);
}

} // namespace eq
