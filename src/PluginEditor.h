#pragma once

#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"

class StepGrid;
class LaneRow;
class Urverket;

class GlappEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlappEditor (GlappProcessor&);
    ~GlappEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectLane (int t);
    void showPresetMenu();

    GlappProcessor& proc;
    ArmLnF lnf;
    juce::TooltipWindow tips { this, 600 };

    gl::Engine view;   // en egen kopia av motorn, bara för att rita

    // toppen
    std::unique_ptr<juce::Component> logo;
    juce::TextButton prevBtn { juce::String::fromUTF8 ("◀") }, nextBtn { juce::String::fromUTF8 ("▶") }, presetBtn;
    ParamCombo stil;
    juce::TextButton diceAllBtn { juce::String::fromUTF8 ("DICE  ·  MUTERA") }, orbitBtn { "ORBIT EVOLVE" }, undoBtn { juce::String::fromUTF8 ("ÅNGRA") };
    DiceButton bigDice;
    ParamToggle spela { "SPELA" }, prov { "PROVLJUD" };
    Knob tempo;

    // spåren
    std::vector<std::unique_ptr<LaneRow>> lanes;
    int selected = 3;

    // höger: urverket och groove
    std::unique_ptr<Urverket> urverk;
    Knob slap, human, provNiva;
    ParamToggle folj { juce::String::fromUTF8 ("FÖLJ TEMPO") }, interlock { "INTERLOCK" };
    ParamCombo ilMode;

    // verkstaden för valt spår
    ParamCombo takt, orbit, orbTakt, hand, handTakt, ackord;
    Knob note, kanal, niva, chans, gate;
    juce::TextButton rensa { "RENSA" };

    juce::Rectangle<int> topArea, laneArea, rightArea, shopArea, grooveArea, urArea;
    AboutOverlay about { "GLAPP", juce::String::fromUTF8 ("MIDI-maskin för mikrosläp och fasdrift") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlappEditor)
};
