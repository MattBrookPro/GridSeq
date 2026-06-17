#include "gridseq/StateCodec.h"

#include "gridseq/SequencerEngine.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace gridseq {

namespace {
constexpr const char* kMagic   = "GRIDSEQ";
constexpr int         kVersion = 1;
} // namespace

// A line-oriented, versioned, human-readable format. WHY text rather than a
// binary blob or JSON: it has zero dependencies, it round-trips exactly (we
// print floats/doubles with enough significant digits to recover the same
// value), and when a test fails the diff is readable by eye. The plugin nests
// this string inside its state alongside the APVTS parameter XML.
std::string serialize(const SequencerEngine& engine)
{
    const Pattern& p = engine.pattern();

    std::ostringstream out;
    out << kMagic << ' ' << kVersion << '\n';

    // 17 sig-figs is enough to round-trip an IEEE-754 double exactly; floats are
    // promoted and recovered to the nearest float on parse.
    out << std::setprecision(17);
    out << "bpm "   << engine.getTempo() << '\n';
    out << "swing " << engine.getSwing() << '\n';
    out << "tracks " << p.numTracks() << '\n';
    out << "steps "  << p.numSteps()  << '\n';

    for (int t = 0; t < p.numTracks(); ++t)
        out << "gain " << t << ' '
            << engine.trackGain_[static_cast<std::size_t>(t)] << '\n';

    // Only active cells are written, in (track, step) order so the output is
    // deterministic - two equal states always serialise to identical bytes.
    for (int t = 0; t < p.numTracks(); ++t)
        for (int s = 0; s < p.numSteps(); ++s)
            if (p.isActive(t, s))
                out << "cell " << t << ' ' << s << ' ' << p.cell(t, s) << '\n';

    return out.str();
}

bool deserialize(SequencerEngine& engine, const std::string& blob)
{
    std::istringstream in(blob);

    // Header gate: refuse anything that is not our format/version. This is what
    // stops a stale or foreign blob from silently half-loading.
    std::string magic;
    int version = 0;
    if (! (in >> magic >> version) || magic != kMagic || version != kVersion)
        return false;

    // Start from a clean slate so loading over an existing kit cannot leave
    // stray cells behind.
    engine.pattern().clear();

    std::string key;
    while (in >> key)
    {
        if (key == "bpm")
        {
            double v; if (! (in >> v)) return false;
            engine.setTempo(v);
        }
        else if (key == "swing")
        {
            double v; if (! (in >> v)) return false;
            engine.setSwing(v);
        }
        else if (key == "tracks")
        {
            int v; if (! (in >> v)) return false;
            engine.pattern().setNumTracks(v);
        }
        else if (key == "steps")
        {
            int v; if (! (in >> v)) return false;
            engine.pattern().setNumSteps(v);
        }
        else if (key == "gain")
        {
            int t; float g; if (! (in >> t >> g)) return false;
            engine.setTrackGain(t, g);
        }
        else if (key == "cell")
        {
            int t, s; float vel; if (! (in >> t >> s >> vel)) return false;
            engine.pattern().setCell(t, s, vel);
        }
        else
        {
            return false; // unknown token => treat as corrupt rather than guess
        }
    }
    return true;
}

} // namespace gridseq
