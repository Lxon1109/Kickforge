cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)
# MSVC: C++-Laufzeit statisch einbinden → die VST braucht keine
# separat installierte VC++-Redistributable (kein "VCRUNTIME140.dll fehlt")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Kickforge VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Nach dem Build automatisch in den System-VST3-Ordner kopieren?
# Braucht unter Windows Adminrechte → standardmäßig aus.
option(KICKFORGE_COPY_AFTER_BUILD "Copy plugin to the system VST3 folder after build" OFF)

# ---------------------------------------------------------------
# JUCE
# Expects JUCE as a submodule or sibling folder. Adjust if needed.
# e.g. git clone https://github.com/juce-framework/JUCE external/JUCE
# ---------------------------------------------------------------
add_subdirectory(external/JUCE)

juce_add_plugin(Kickforge
    COMPANY_NAME        "Forgeworks"
    PLUGIN_MANUFACTURER_CODE  Fgwx
    PLUGIN_CODE         Kfrg
    FORMATS             VST3 Standalone
    PRODUCT_NAME        "Kickforge"
    IS_SYNTH            TRUE
    NEEDS_MIDI_INPUT    TRUE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${KICKFORGE_COPY_AFTER_BUILD}
    VST3_CATEGORIES     "Instrument" "Drum"
)

juce_generate_juce_header(Kickforge)

# Eingebettete Ressourcen (Font + Logo) → BinaryData::
juce_add_binary_data(KickforgeAssets SOURCES
    Resources/Fonts/ChakraPetch-Regular.ttf
    Resources/Fonts/ChakraPetch-SemiBold.ttf
    Resources/Fonts/ChakraPetch-Bold.ttf
    Resources/Logo/emblem.svg
)

target_sources(Kickforge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/KickforgeLookAndFeel.cpp
    Source/KickforgeLookAndFeel.h
    Source/Theme.h
    Source/KickVoice.cpp
    Source/KickVoice.h
    Source/KickLayer.cpp
    Source/KickLayer.h
    Source/Saturation.h
    Source/SvfFilter.h
    Source/MasterEQ.h
    Source/MasterReverb.h
    Source/TransientShaper.h
    Source/Presets.h
    Source/WaveformDisplay.h
)

target_compile_options(Kickforge PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(Kickforge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Kickforge PRIVATE
    KickforgeAssets
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------
# DSP-Tests (optional):  cmake -DKICKFORGE_BUILD_TESTS=ON ...
# ---------------------------------------------------------------
option(KICKFORGE_BUILD_TESTS "Build Kickforge DSP tests" OFF)
if(KICKFORGE_BUILD_TESTS)
    juce_add_console_app(KickforgeTests PRODUCT_NAME "KickforgeTests")
    target_sources(KickforgeTests PRIVATE
        Tests/DspTests.cpp
        Source/KickVoice.cpp
        Source/KickLayer.cpp
    )
    target_compile_definitions(KickforgeTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_compile_options(KickforgeTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(KickforgeTests PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
endif()
