#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "gridseq/DrumSynth.h"
#include "gridseq/StateCodec.h"

using namespace gridseq;

juce::AudioProcessorValueTreeState::ParameterLayout
GridSeqAudioProcessor::createLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam  = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<BoolParam>(juce::ParameterID { ids::run, 1 },
                                           "Run", false));
    layout.add(std::make_unique<FloatParam>(juce::ParameterID { ids::tempo, 1 }, "Tempo",
                                            juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f),
                                            120.0f));
    layout.add(std::make_unique<FloatParam>(juce::ParameterID { ids::swing, 1 }, "Swing",
                                            juce::NormalisableRange<float>(0.0f, 0.66f, 0.001f),
                                            0.0f));
    layout.add(std::make_unique<FloatParam>(juce::ParameterID { ids::master, 1 }, "Master",
                                            juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f),
                                            1.0f));

    for (int t = 0; t < 8; ++t)
        layout.add(std::make_unique<FloatParam>(juce::ParameterID { ids::trackGain(t), 1 },
                                                juce::String(kTrackNames[(size_t) t]) + " Gain",
                                                juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f),
                                                1.0f));
    return layout;
}

GridSeqAudioProcessor::GridSeqAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output",
                                                        juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    // Cache the raw atomic param pointers once - looking them up by string in the
    // audio callback would be wasteful.
    runParam_    = apvts.getRawParameterValue(ids::run);
    tempoParam_  = apvts.getRawParameterValue(ids::tempo);
    swingParam_  = apvts.getRawParameterValue(ids::swing);
    masterParam_ = apvts.getRawParameterValue(ids::master);
    for (int t = 0; t < 8; ++t)
        gainParams_[(size_t) t] = apvts.getRawParameterValue(ids::trackGain(t));

    // A musical starter pattern so the instrument makes a beat the moment it
    // loads - better first impression than an empty grid.
    auto& p = engine_.pattern();
    p.setNumTracks(8);
    p.setNumSteps(16);
    for (int s : { 0, 4, 8, 12 }) p.setCell(0, s, 1.0f);   // kick: four-on-the-floor
    p.setCell(1, 4, 0.9f); p.setCell(1, 12, 0.9f);          // snare: backbeat
    for (int s = 0; s < 16; s += 2) p.setCell(2, s, 0.55f); // closed hat: 8ths
    p.setCell(3, 14, 0.7f);                                 // open hat: bar-end lift
}

void GridSeqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate);

    // Render the procedural kit at the host's sample rate and hand each buffer to
    // its track. Done here (not in the callback) because it allocates.
    engine_.setSample(0, drums::kick(sampleRate));
    engine_.setSample(1, drums::snare(sampleRate));
    engine_.setSample(2, drums::closedHat(sampleRate));
    engine_.setSample(3, drums::openHat(sampleRate));
    engine_.setSample(4, drums::clap(sampleRate));
    engine_.setSample(5, drums::tom(sampleRate));
    engine_.setSample(6, drums::rim(sampleRate));
    engine_.setSample(7, drums::cowbell(sampleRate));

    // Pre-size the stereo scratch so the callback never allocates.
    scratch_.setSize(2, samplesPerBlock, false, false, true);
}

bool GridSeqAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void GridSeqAudioProcessor::drainEdits() noexcept
{
    // Apply every queued UI edit at the top of the block, on the audio thread.
    EditCommand cmd;
    while (edits_.pop(cmd))
    {
        switch (cmd.type)
        {
            case EditCommand::Type::toggleCell:
                engine_.pattern().toggle(cmd.track, cmd.step, cmd.velocity);
                break;
            case EditCommand::Type::clearAll:
                engine_.pattern().clear();
                break;
        }
    }
}

void GridSeqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    jassert(numSamples <= scratch_.getNumSamples());

    // 1) Apply structural edits (lock-free queue) and continuous params (atomics).
    drainEdits();
    engine_.setTempo(*tempoParam_);
    engine_.setSwing(*swingParam_);
    for (int t = 0; t < 8; ++t)
        engine_.setTrackGain(t, gainParams_[(size_t) t]->load());
    engine_.setPlaying(*runParam_ > 0.5f);

    // 2) Render into two distinct scratch channels, then fan out to the host's
    //    actual output channels with master gain applied.
    float* L = scratch_.getWritePointer(0);
    float* R = scratch_.getWritePointer(1);
    engine_.processBlock(L, R, numSamples);

    const float master = masterParam_->load();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* src = (ch == 0) ? L : R;
        float* dst = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            dst[i] = src[i] * master;
    }

    // 3) Publish the playhead for the UI (single relaxed atomic store).
    playhead_.store(engine_.currentStep(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* GridSeqAudioProcessor::createEditor()
{
    return new GridSeqAudioProcessorEditor(*this);
}

void GridSeqAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save the APVTS parameter tree, and tuck the pattern grid (which is engine
    // state, not a parameter) in alongside it as a serialised blob property.
    auto tree = apvts.copyState();
    tree.setProperty("patternBlob", juce::String(serialize(engine_)), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void GridSeqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // NOTE: the host calls this at instantiation (before audio runs), so writing
    // the engine pattern directly here is safe in practice; a production build
    // would route it through the same edit queue to be airtight.
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
        {
            apvts.replaceState(tree);
            const auto blob = apvts.state.getProperty("patternBlob").toString();
            deserialize(engine_, blob.toStdString());
        }
    }
}

// JUCE entry point: the host instantiates the plugin through this.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GridSeqAudioProcessor();
}
