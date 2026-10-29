// Test av hela pluginet utan DAW: kör processBlock med en låtsas-DAW,
// kontrollerar MIDI-utgången, state, tärning/ångra och ritar GUI:t till en PNG.
// Byggs med:  cmake -B build -DGLAPP_TESTS=ON && cmake --build build --target GlappTest

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

using namespace gl;

static int fails = 0;
#define CHECK(x) do { if (! (x)) { std::printf ("FEL: %s (rad %d)\n", #x, __LINE__); ++fails; } } while (0)

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 122.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    GlappProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    const double sr = 48000.0;
    const int bs = 512;
    proc.prepareToPlay (sr, bs);

    juce::AudioBuffer<float> buf (2, bs);
    int ons = 0, offs = 0, kicks = 0, kickOffGrid = 0;
    std::map<int, int> open;
    const double pps = host.bpm / 60.0 / sr;
    int64_t sample = 0;

    auto run = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                const int key = msg.getChannel() * 128 + msg.getNoteNumber();
                if (msg.isNoteOn())
                {
                    ++ons; ++open[key];
                    if (msg.getNoteNumber() == 36)
                    {
                        ++kicks;
                        const double ppq = (double) (sample + m.samplePosition) * pps;
                        const double steps = ppq / 0.25;
                        if (std::abs (steps - std::round (steps)) * 0.25 / pps > 1.01) ++kickOffGrid;   // mer än 1 sample fel
                    }
                }
                else if (msg.isNoteOff()) { ++offs; --open[key]; }
            }
            sample += bs;
            host.ppq = (double) sample * pps;
        }
    };

    // 8 takter vid 122 BPM
    const int blocks8 = (int) std::ceil (32.0 / pps / bs);
    run (blocks8);
    std::printf ("8 takter: %d noter, %d kickar\n", ons, kicks);
    CHECK (ons > 50);
    CHECK (kicks >= 28 && kicks <= 34);
    CHECK (kickOffGrid == 0);

    // stoppa: alla noter ska släppas
    host.playing = false;
    run (2);
    int stillOpen = 0;
    for (auto& kv : open) stillOpen += kv.second;
    CHECK (stillOpen == 0);
    CHECK (ons == offs);

    // state rundtur
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    const auto before = proc.pattern.trackToString (RPerk1);
    proc.diceAll();
    CHECK (proc.canUndo());
    proc.undo();
    CHECK (proc.pattern.trackToString (RPerk1) == before);
    proc.diceAll();
    proc.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (proc.pattern.trackToString (RPerk1) == before);

    // alla presets laddar
    for (int i = 0; i < NUM_PRESETS; ++i) proc.setCurrentProgram (i);
    CHECK (proc.getCurrentProgram() == NUM_PRESETS - 1);
    proc.setCurrentProgram (3);

    // GUI till PNG
    host.playing = true;
    run (40);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* ge = dynamic_cast<GlappEditor*> (ed.get());
        CHECK (ge != nullptr);
        ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("glapp.png"));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("GUI ritat till %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }

    std::printf (fails == 0 ? "ALLT OK\n" : "%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
