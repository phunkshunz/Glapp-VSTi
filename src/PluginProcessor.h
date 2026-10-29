#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Engine.h"
#include "Dice.h"
#include "Prov.h"

class GlappProcessor : public juce::AudioProcessor
{
public:
    GlappProcessor();
    ~GlappProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GLAPP"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return gl::NUM_PRESETS; }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override { return juce::String::fromUTF8 (gl::presetName (index).c_str()); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- för GUI:t (meddelandetråden)
    juce::AudioProcessorValueTreeState apvts;
    gl::Pattern pattern;

    void readConfig (gl::Engine& e) const;          // fyller en motor med aktuella inställningar
    void diceAll();                                  // DICE: alla spår som inte är frysta
    void diceLane (int t);                           // tärningen på ett spår
    void orbitEvolve();                              // nya rörelser, samma mönster
    void loadPreset (int index);
    void clearLane (int t);
    bool undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void pushUndo();
    void setStep (int t, int i, gl::Step s) { pattern.set (t, i, s); }

    juce::String presetLabel() const;

    std::atomic<double> uiPpq { 0.0 }, uiBpm { 122.0 }, uiBarPpq { 4.0 };
    std::atomic<bool> uiPlaying { false };
    std::array<std::atomic<int>, gl::NT> uiHits {};
    std::atomic<int> currentPreset { -1 };

private:
    void setParam (const juce::String& id, float value);
    void applyTrack (int t, const gl::TrackGen& g);
    juce::ValueTree snapshot();
    void restore (const juce::ValueTree& tree);

    struct RawTrack
    {
        std::atomic<float>* p[17] {};
    };
    std::array<RawTrack, gl::NT> raw;
    std::atomic<float> *pSlap, *pHuman, *pFolj, *pIl, *pIlMode, *pProv, *pProvNiva, *pSpela, *pTempo;
    std::atomic<uint32_t> seed { 1 };

    gl::Engine engine;
    gl::Prov prov;
    std::vector<gl::NoteEv> events;

    struct PendingOff { int note, channel; int64_t at; };
    std::vector<PendingOff> pending;
    int64_t sampleClock = 0;
    double expectedPpq = -1.0e9;
    bool wasRunning = false;
    double internalPpq = 0.0;
    bool internalWasOn = false;
    double sr = 48000.0;

    std::vector<juce::ValueTree> undoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlappProcessor)
};
