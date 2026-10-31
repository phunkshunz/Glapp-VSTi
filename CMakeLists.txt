cmake_minimum_required(VERSION 3.22)

project(GLAPP VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE hämtas automatiskt vid bygget (eller från JUCE_DIR om den finns lokalt)
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# GLAPP gör inget eget ljud (utom provljudet). Den skickar MIDI ut till nästa
# instrument, därför är den ett instrument med MIDI-utgång.
# Linux: bygg även LV2
set(PHZ_EXTRA_FORMATS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PHZ_EXTRA_FORMATS LV2)
endif()
juce_add_plugin(Glapp
    COMPANY_NAME "Phunkshunz AUDIO"
    PRODUCT_NAME "GLAPP"
    BUNDLE_ID "com.phunkshunz.glapp"
    PLUGIN_MANUFACTURER_CODE Phnk
    PLUGIN_CODE Glap
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "https://phunkshunz.github.io/lv2/Glapp"
    FORMATS VST3 AU Standalone ${PHZ_EXTRA_FORMATS})

target_sources(Glapp
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp)

target_compile_definitions(Glapp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Glapp
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Linux används bara för utveckling och tester: inget ljudkort, inga X-tillägg
if(UNIX AND NOT APPLE AND TARGET Glapp)
    target_compile_definitions(Glapp PUBLIC JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0)
endif()

# Tester (byggs bara när man ber om det): cmake -B build -DGLAPP_TESTS=ON
option(GLAPP_TESTS "Bygg GLAPP-testerna" OFF)
if(GLAPP_TESTS)
    juce_add_console_app(GlappTest PRODUCT_NAME "GlappTest")
    target_sources(GlappTest PRIVATE tests/plugin_test.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_compile_definitions(GlappTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(GlappTest PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()
