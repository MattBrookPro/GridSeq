#include "PluginEditor.h"

namespace {
constexpr int kNumTracks = 8;
constexpr float kGroupGap = 8.0f; // visual gap between beats (groups of 4 steps)

// One colour per instrument row - colour-coding the rows is a big readability win
// when you're scanning a pattern. Order matches kTrackNames / the kit.
const std::array<juce::Colour, 8> kTrackColours {
    juce::Colour(0xffe53935), // Kick   - red
    juce::Colour(0xfffb8c00), // Snare  - orange
    juce::Colour(0xfffdd835), // CH     - yellow
    juce::Colour(0xff43a047), // OH     - green
    juce::Colour(0xff00acc1), // Clap   - cyan
    juce::Colour(0xff3949ab), // Tom    - indigo
    juce::Colour(0xff8e24aa), // Rim    - purple
    juce::Colour(0xffd81b60), // Cowb   - pink
};

const juce::Colour kBg       { 0xff1b1b1f };
const juce::Colour kCellOff  { 0xff2c2c32 };
const juce::Colour kCellOff2 { 0xff35353d }; // alternate beat-group shade
} // namespace

// -- StepGridComponent ---------------------------------------------------------
StepGridComponent::StepGridComponent(GridSeqAudioProcessor& p) : processor_(p)
{
    // 30 Hz is plenty to make the moving playhead feel live without burdening the
    // UI thread.
    startTimerHz(30);
}

StepGridComponent::~StepGridComponent() { stopTimer(); }

juce::Rectangle<float> StepGridComponent::cellBounds(int track, int step) const
{
    const auto area  = getLocalBounds().toFloat().reduced(4.0f);
    const int  steps = processor_.pattern().numSteps();
    const int  groups = (steps + 3) / 4;

    const float usableW = area.getWidth() - kGroupGap * (float) (groups - 1);
    const float cellW   = usableW / (float) steps;
    const float cellH   = area.getHeight() / (float) kNumTracks;

    const float x = area.getX() + (float) step * cellW + (float) (step / 4) * kGroupGap;
    const float y = area.getY() + (float) track * cellH;
    return juce::Rectangle<float>(x, y, cellW, cellH).reduced(2.0f);
}

void StepGridComponent::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

    const auto& pat   = processor_.pattern();
    const int steps   = pat.numSteps();
    const int playhead = processor_.currentStep();

    for (int t = 0; t < kNumTracks; ++t)
    {
        for (int s = 0; s < steps; ++s)
        {
            const auto cb  = cellBounds(t, s);
            const float vel = pat.cell(t, s);

            if (vel > 0.0f)
            {
                // Lit cell: the row colour, brightened by velocity so accents read
                // visually as well as audibly.
                g.setColour(kTrackColours[(size_t) t].withMultipliedBrightness(0.6f + 0.4f * vel));
            }
            else
            {
                // Off cell: alternate the shade per group of four so the beat is
                // easy to find at a glance.
                g.setColour((s / 4) % 2 == 0 ? kCellOff : kCellOff2);
            }
            g.fillRoundedRectangle(cb, 3.0f);
        }
    }

    // Playhead: a translucent column over the step currently sounding.
    if (playhead >= 0 && playhead < steps)
    {
        const auto colRect = cellBounds(0, playhead)
                                 .getUnion(cellBounds(kNumTracks - 1, playhead))
                                 .expanded(2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.fillRoundedRectangle(colRect, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawRoundedRectangle(colRect, 4.0f, 1.5f);
    }
}

void StepGridComponent::mouseDown(const juce::MouseEvent& e)
{
    const int steps = processor_.pattern().numSteps();
    for (int t = 0; t < kNumTracks; ++t)
        for (int s = 0; s < steps; ++s)
            if (cellBounds(t, s).contains(e.position))
            {
                // Post the edit to the audio thread; repaint optimistically so the
                // click feels instant rather than waiting a frame for the engine.
                processor_.postEdit({ EditCommand::Type::toggleCell, t, s, 0.8f });
                repaint();
                return;
            }
}

void StepGridComponent::timerCallback()
{
    const int s = processor_.currentStep();
    if (s != lastPlayhead_)
    {
        lastPlayhead_ = s;
        repaint(); // only when the playhead actually moves
    }
}

// -- Editor --------------------------------------------------------------------
GridSeqAudioProcessorEditor::GridSeqAudioProcessorEditor(GridSeqAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor_(p), grid_(p)
{
    addAndMakeVisible(grid_);

    runButton_.setClickingTogglesState(true);
    runButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff43a047));
    addAndMakeVisible(runButton_);
    runAtt_ = std::make_unique<BAttach>(processor_.state(), ids::run, runButton_);

    auto setupBar = [this](juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
        addAndMakeVisible(s);
        l.setText(name, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        addAndMakeVisible(l);
    };
    setupBar(tempoSlider_,  tempoLabel_,  "Tempo");
    setupBar(swingSlider_,  swingLabel_,  "Swing");
    setupBar(masterSlider_, masterLabel_, "Level");
    tempoAtt_  = std::make_unique<SAttach>(processor_.state(), ids::tempo,  tempoSlider_);
    swingAtt_  = std::make_unique<SAttach>(processor_.state(), ids::swing,  swingSlider_);
    masterAtt_ = std::make_unique<SAttach>(processor_.state(), ids::master, masterSlider_);

    for (int t = 0; t < 8; ++t)
    {
        auto& knob = trackGain_[(size_t) t];
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setColour(juce::Slider::rotarySliderFillColourId, kTrackColours[(size_t) t]);
        addAndMakeVisible(knob);
        gainAtt_[(size_t) t] = std::make_unique<SAttach>(processor_.state(), ids::trackGain(t), knob);

        auto& lab = trackLabel_[(size_t) t];
        lab.setText(kTrackNames[(size_t) t], juce::dontSendNotification);
        lab.setJustificationType(juce::Justification::centred);
        lab.setColour(juce::Label::textColourId, kTrackColours[(size_t) t]);
        lab.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        addAndMakeVisible(lab);
    }

    setSize(800, 460);
}

void GridSeqAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg.darker(0.3f));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    g.drawText("GridSeq", 12, 8, 200, 28, juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(12.0f);
    g.drawText("step sequencer", 120, 14, 200, 18, juce::Justification::centredLeft);
}

void GridSeqAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Top control bar: Run + Tempo / Swing / Level.
    auto top = area.removeFromTop(44);
    top.removeFromLeft(220); // leave room for the title drawn in paint()
    runButton_.setBounds(top.removeFromLeft(70).reduced(4));

    // Split the remaining bar into three EQUAL strips. Work out the width once up
    // front; using top.getWidth() / 3 on each call divides a smaller leftover each
    // time, so the later sliders and their labels end up squashed.
    const int barW = top.getWidth() / 3;
    auto placeBar = [&top, barW](juce::Label& l, juce::Slider& s)
    {
        auto strip = top.removeFromLeft(barW).reduced(4, 4);
        l.setBounds(strip.removeFromLeft(48));
        s.setBounds(strip);
    };
    placeBar(tempoLabel_,  tempoSlider_);
    placeBar(swingLabel_,  swingSlider_);
    placeBar(masterLabel_, masterSlider_);

    area.removeFromTop(6);

    // Left rail of per-track name + gain knob, aligned to the grid rows.
    auto rail = area.removeFromLeft(60);
    grid_.setBounds(area);

    const float rowH = (float) area.getHeight() / (float) kNumTracks;
    for (int t = 0; t < 8; ++t)
    {
        auto row = juce::Rectangle<int>(rail.getX(),
                                        area.getY() + (int) ((float) t * rowH),
                                        rail.getWidth(),
                                        (int) rowH).reduced(2);
        trackLabel_[(size_t) t].setBounds(row.removeFromTop(16));
        trackGain_[(size_t) t].setBounds(row);
    }
}
