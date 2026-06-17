#pragma once

#include <string>

namespace gridseq {

class SequencerEngine;

// State save/load for the engine, kept as free functions in their own unit so
// the serialisation format is testable on its own and the engine header stays
// focused on real-time behaviour.
//
// The format is a deliberately simple, human-readable, versioned text blob
// (not JSON - no dependency, and easy to eyeball in a test failure). The plugin
// embeds this string inside its getStateInformation()/setStateInformation()
// alongside the APVTS parameter XML.
//
// The contract that matters for the test suite: deserialize(serialize(e)) must
// reproduce tempo, swing, geometry and every pattern cell exactly (round-trip).
std::string serialize(const SequencerEngine& engine);
bool        deserialize(SequencerEngine& engine, const std::string& blob);

} // namespace gridseq
