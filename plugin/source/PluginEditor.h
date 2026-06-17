#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

// -- StepGridComponent ---------------------------------------------------------
// The heart of the UI: the editable step grid. It is designed around how a beat
// is actually made - rows are instruments, columns are 16th-note steps grouped in
// fours so the eye finds the beat, the playing column is lit so you can see the
// groove move, and a single click toggles a step. Clicks are posted to the audio
// thread via the processor's lock-free queue; the grid never touches engine state
// directly except to *read* it for drawing.
class StepGridComponent : public juce::Component,
                          private juce::Timer
{
public:
    explicit StepGridComponent(GridSeqAudioProcessor& p);
    ~StepGridComponent() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> cellBounds(int track, int step) const;

    GridSeqAudioProcessor& processor_;
    int lastPlayhead_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepGridComponent)
};

// -- Editor --------------------------------------------------------------------
class GridSeqAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GridSeqAudioProcessorEditor(GridSeqAudioProcessor&);
    ~GridSeqAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS   = juce::AudioProcessorValueTreeState;
    using SAttach = APVTS::SliderAttachment;
    using BAttach = APVTS::ButtonAttachment;

    GridSeqAudioProcessor& processor_;

    StepGridComponent grid_;

    juce::TextButton runButton_ { "Run" };
    juce::Slider     tempoSlider_, swingSlider_, masterSlider_;
    juce::Label      tempoLabel_, swingLabel_, masterLabel_;

    std::array<juce::Slider, 8> trackGain_;
    std::array<juce::Label, 8>  trackLabel_;

    std::unique_ptr<BAttach> runAtt_;
    std::unique_ptr<SAttach> tempoAtt_, swingAtt_, masterAtt_;
    std::array<std::unique_ptr<SAttach>, 8> gainAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridSeqAudioProcessorEditor)
};
