#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"

namespace gl
{
inline juce::String tid (int t, const char* key) { return "t" + juce::String (t) + "_" + key; }

inline juce::StringArray toSA (const std::vector<std::string>& v)
{
    juce::StringArray a;
    for (auto& s : v) a.add (juce::String::fromUTF8 (s.c_str()));
    return a;
}

inline juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }

// Standardnoter: C1 och uppåt = pad 1-8 i BETONG, kanal 1. Lägg GLAPP framför BETONG och kör.
inline int defaultNote (int t) { return 36 + t; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    const auto U = [] (const char* s) { return String::fromUTF8 (s); };

    auto ms = [] (float v, int) { return String (v, 1) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

    for (int t = 0; t < NT; ++t)
    {
        const String pre = String (roleName (t)) + " ";
        L.add (std::make_unique<AudioParameterBool> (ParameterID { tid (t, "tyst"), 1 }, pre + "TYST", false));
        L.add (std::make_unique<AudioParameterBool> (ParameterID { tid (t, "frys"), 1 }, pre + "FRYS", false));
        L.add (std::make_unique<AudioParameterInt> (ParameterID { tid (t, "langd"), 1 }, pre + U ("LÄNGD"), 1, MAXS, 16));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "takt"), 1 }, pre + "TAKT", toSA (divNames()), (int) D16));
        L.add (std::make_unique<AudioParameterInt> (ParameterID { tid (t, "not"), 1 }, pre + "NOT", 0, 127, defaultNote (t),
                                                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
        L.add (std::make_unique<AudioParameterInt> (ParameterID { tid (t, "kanal"), 1 }, pre + "KANAL", 1, 16, 1));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "niva"), 1 }, pre + U ("NIVÅ"), NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "drag"), 1 }, pre + "DRAG", NormalisableRange<float> (0.0f, 60.0f, 0.1f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "trog"), 1 }, pre + U ("TRÖGHET"), NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "jitter"), 1 }, pre + "JITTER", NormalisableRange<float> (0.0f, 5.0f, 0.1f), 0.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "chans"), 1 }, pre + "CHANS", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { tid (t, "gate"), 1 }, pre + U ("NOTLÄNGD"), NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.3f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "orbit"), 1 }, pre + "ORBIT", toSA (orbitNames()), 0));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "orbtakt"), 1 }, pre + "ORBIT TAKTER", toSA (barsNames()), 2));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "hand"), 1 }, pre + U ("HÄNDELSE"), toSA (eventNames()), 0));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "handtakt"), 1 }, pre + U ("HÄNDELSE TAKT"), toSA (barsNames()), 3));
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { tid (t, "ackord"), 1 }, pre + "ACKORD", toSA (chordNames()), 0));
    }

    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "slap", 1 }, U ("SLÄP"), NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "human", 1 }, "HUMAN", NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "foljtempo", 1 }, U ("FÖLJ TEMPO"), true));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "interlock", 1 }, "INTERLOCK", true));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "ilmode", 1 }, U ("INTERLOCK LÄGE"), StringArray { U ("SPÖKE"), "FLYTTA", "TYST" }, 0));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "stil", 1 }, "STIL", toSA (styleNames()), 0));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "prov", 1 }, "PROVLJUD", false));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "provniva", 1 }, U ("PROVLJUD NIVÅ"), NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    L.add (std::make_unique<AudioParameterBool> (ParameterID { "spela", 1 }, "SPELA", false));
    L.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "TEMPO", NormalisableRange<float> (60.0f, 180.0f, 0.1f), 122.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " BPM"; })));
    return L;
}
} // namespace gl
