#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace gridseq::drums {

// Local pi - M_PI is non-standard and we build with compiler extensions off.
inline constexpr double kPi = 3.14159265358979323846;

// Procedurally-synthesised drum one-shots. WHY synthesise rather than ship audio
// files: it keeps the repo free of any sample-licensing question, makes the
// "kit" fully reproducible (a fixed RNG seed -> identical bytes every run, which
// the Python regression tool relies on), and it is itself a small bit of audio
// DSP I can talk through. These are plain mono float buffers the SamplerVoice
// plays back - the same buffers the unit tests can inspect.
//
// All generators are pure functions of the sample rate. They live in the engine
// (not the plugin) so they are covered by the test suite and the plugin stays a
// thin shell.

namespace detail {

// A tiny, deterministic xorshift PRNG. Using our own (rather than <random>)
// guarantees the SAME noise on every platform and compiler - std::mt19937 is
// portable but the distributions are not, and bit-identical output is what makes
// the kit a stable regression fixture.
class WhiteNoise
{
public:
    explicit WhiteNoise(std::uint32_t seed) : state_(seed ? seed : 0x1234567u) {}
    float next() noexcept
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        // Map to [-1, 1).
        return static_cast<float>(state_) / 2147483648.0f - 1.0f;
    }
private:
    std::uint32_t state_;
};

inline int durationSamples(double sr, double seconds)
{
    return static_cast<int>(sr * seconds);
}

// Exponential decay envelope value at sample i (1.0 -> ~0 over `samples`).
inline float expEnv(int i, int samples, float curve = 5.0f)
{
    const float t = static_cast<float>(i) / static_cast<float>(samples);
    return std::exp(-curve * t);
}

} // namespace detail

// Kick: a pitch-swept sine (punchy attack falling to sub) with a fast amplitude
// decay - the classic synthesised 808/909-style kick.
inline std::vector<float> kick(double sr)
{
    const int n = detail::durationSamples(sr, 0.32);
    std::vector<float> out(static_cast<std::size_t>(n));
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float env  = detail::expEnv(i, n, 5.5f);
        const float t    = static_cast<float>(i) / static_cast<float>(n);
        const double freq = 120.0 * std::exp(-8.0 * t) + 45.0; // 120Hz -> ~45Hz
        phase += 2.0 * kPi * freq / sr;
        out[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(phase)) * env * 0.9f;
    }
    return out;
}

// Snare: band-ish noise + a 185 Hz body tone, medium decay.
inline std::vector<float> snare(double sr)
{
    const int n = detail::durationSamples(sr, 0.20);
    std::vector<float> out(static_cast<std::size_t>(n));
    detail::WhiteNoise noise(0x5A17E);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float env = detail::expEnv(i, n, 7.0f);
        phase += 2.0 * kPi * 185.0 / sr;
        const float body = std::sin(static_cast<float>(phase)) * 0.4f;
        const float hiss = noise.next() * 0.7f;
        out[static_cast<std::size_t>(i)] = (body + hiss) * env * 0.7f;
    }
    return out;
}

// Hi-hats: high-passed-feeling noise (we approximate by differencing the noise),
// short for closed, longer for open.
inline std::vector<float> hat(double sr, double seconds, std::uint32_t seed)
{
    const int n = detail::durationSamples(sr, seconds);
    std::vector<float> out(static_cast<std::size_t>(n));
    detail::WhiteNoise noise(seed);
    float prev = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float env = detail::expEnv(i, n, 9.0f);
        const float w   = noise.next();
        const float hp  = w - prev; // crude high-pass -> brighter, metallic
        prev = w;
        out[static_cast<std::size_t>(i)] = hp * env * 0.5f;
    }
    return out;
}

inline std::vector<float> closedHat(double sr) { return hat(sr, 0.05, 0xC10);  }
inline std::vector<float> openHat(double sr)   { return hat(sr, 0.30, 0x09E11); }

// Clap: three quick noise bursts then a short tail - the stacked-transient trick.
inline std::vector<float> clap(double sr)
{
    const int n = detail::durationSamples(sr, 0.18);
    std::vector<float> out(static_cast<std::size_t>(n));
    detail::WhiteNoise noise(0xC1A9);
    const int burst = detail::durationSamples(sr, 0.008);
    for (int i = 0; i < n; ++i)
    {
        float env;
        if (i < burst * 3) // three closely-spaced transients
            env = (i / burst) % 2 == 0 ? 1.0f : 0.3f;
        else
            env = detail::expEnv(i - burst * 3, n, 6.0f);
        out[static_cast<std::size_t>(i)] = noise.next() * env * 0.6f;
    }
    return out;
}

// Tom: a sine with a slight downward pitch glide.
inline std::vector<float> tom(double sr, double startFreq = 160.0)
{
    const int n = detail::durationSamples(sr, 0.30);
    std::vector<float> out(static_cast<std::size_t>(n));
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float t   = static_cast<float>(i) / static_cast<float>(n);
        const float env = detail::expEnv(i, n, 5.0f);
        const double freq = startFreq * std::exp(-1.5 * t);
        phase += 2.0 * kPi * freq / sr;
        out[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(phase)) * env * 0.8f;
    }
    return out;
}

// Rim/click: very short bright transient.
inline std::vector<float> rim(double sr)
{
    const int n = detail::durationSamples(sr, 0.03);
    std::vector<float> out(static_cast<std::size_t>(n));
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float env = detail::expEnv(i, n, 20.0f);
        phase += 2.0 * kPi * 1700.0 / sr;
        out[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(phase)) * env * 0.7f;
    }
    return out;
}

// Cowbell: two detuned square-ish tones - the unmistakable "more cowbell".
inline std::vector<float> cowbell(double sr)
{
    const int n = detail::durationSamples(sr, 0.25);
    std::vector<float> out(static_cast<std::size_t>(n));
    double p1 = 0.0, p2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float env = detail::expEnv(i, n, 6.0f);
        p1 += 2.0 * kPi * 540.0 / sr;
        p2 += 2.0 * kPi * 800.0 / sr;
        const float s = (std::sin(static_cast<float>(p1)) > 0 ? 0.5f : -0.5f)
                      + (std::sin(static_cast<float>(p2)) > 0 ? 0.3f : -0.3f);
        out[static_cast<std::size_t>(i)] = s * env * 0.5f;
    }
    return out;
}

} // namespace gridseq::drums
