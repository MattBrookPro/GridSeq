// gridseq_render - render a pattern through the engine to a mono 16-bit WAV,
// offline and headless. WHY this exists: it lets a script (see qa_render_check.py)
// drive the real engine, capture its output, and verify that hits land on the
// expected samples - the same QA discipline as the unit tests, but at the
// rendered-audio level and usable in a delivery pipeline.
//
// Two sound modes:
//   default  : each used track plays a one-sample impulse, so onsets are exact
//              single samples - ideal for a precise regression check.
//   --kit    : the procedural drum kit, for actually listening to the output.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "gridseq/DrumSynth.h"
#include "gridseq/SequencerEngine.h"

using namespace gridseq;

namespace {

struct Cell { int track; int step; };

void writeWavMono16(const std::string& path, const std::vector<float>& samples, int sampleRate)
{
    std::ofstream f(path, std::ios::binary);
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
    const std::uint32_t byteRate  = static_cast<std::uint32_t>(sampleRate) * 2;

    auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };

    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(1);            // PCM, mono
    u32(static_cast<std::uint32_t>(sampleRate)); u32(byteRate); u16(2); u16(16);
    f.write("data", 4); u32(dataBytes);

    for (float s : samples)
    {
        const float c = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
        u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(c * 32767.0f))));
    }
}

[[noreturn]] void usage()
{
    std::fprintf(stderr,
        "usage: gridseq_render --out FILE [--sr N] [--bpm N] [--swing F]\n"
        "                      [--steps N] [--bars N] [--kit] [--cell T:S ...]\n");
    std::exit(2);
}

} // namespace

int main(int argc, char** argv)
{
    std::string out;
    int    sr    = 48000;
    double bpm   = 120.0;
    double swing = 0.0;
    int    steps = 16;
    int    bars  = 1;
    bool   useKit = false;
    std::vector<Cell> cells;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) usage(); return argv[++i]; };

        if      (a == "--out")   out   = next();
        else if (a == "--sr")    sr    = std::stoi(next());
        else if (a == "--bpm")   bpm   = std::stod(next());
        else if (a == "--swing") swing = std::stod(next());
        else if (a == "--steps") steps = std::stoi(next());
        else if (a == "--bars")  bars  = std::stoi(next());
        else if (a == "--kit")   useKit = true;
        else if (a == "--cell")
        {
            const std::string v = next();
            const auto colon = v.find(':');
            if (colon == std::string::npos) usage();
            cells.push_back({ std::stoi(v.substr(0, colon)), std::stoi(v.substr(colon + 1)) });
        }
        else usage();
    }
    if (out.empty()) usage();

    SequencerEngine engine;
    engine.prepare(static_cast<double>(sr));
    engine.setTempo(bpm);
    engine.setSwing(swing);
    engine.pattern().setNumSteps(steps);

    // Load sounds: kit, or exact impulses for sample-accurate onset checking.
    for (int t = 0; t < Pattern::kMaxTracks; ++t)
        engine.setSample(t, useKit ? std::vector<float>{} : std::vector<float>{ 1.0f });
    if (useKit)
    {
        engine.setSample(0, drums::kick(sr));      engine.setSample(1, drums::snare(sr));
        engine.setSample(2, drums::closedHat(sr)); engine.setSample(3, drums::openHat(sr));
        engine.setSample(4, drums::clap(sr));      engine.setSample(5, drums::tom(sr));
        engine.setSample(6, drums::rim(sr));       engine.setSample(7, drums::cowbell(sr));
    }

    for (const auto& c : cells)
        engine.pattern().setCell(c.track, c.step, 1.0f);

    engine.setPlaying(true);

    // One step = (60/bpm)/4 seconds (16th notes). Render whole bars.
    const double samplesPerStep = (60.0 / bpm) / 4.0 * sr;
    const long total = static_cast<long>(std::llround(samplesPerStep * steps * bars));

    std::vector<float> mono;
    mono.reserve(static_cast<std::size_t>(total));

    const int blockSize = 512;
    std::vector<float> L(blockSize), R(blockSize);
    for (long done = 0; done < total; done += blockSize)
    {
        const int n = static_cast<int>(std::min<long>(blockSize, total - done));
        engine.processBlock(L.data(), R.data(), n);
        mono.insert(mono.end(), L.begin(), L.begin() + n); // left channel only
    }

    writeWavMono16(out, mono, sr);
    std::printf("rendered %ld samples @ %d Hz -> %s\n", total, sr, out.c_str());
    return 0;
}
