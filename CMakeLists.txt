cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)
# MSVC: C++-Laufzeit statisch einbinden → die VST braucht keine
# separat installierte VC++-Redistributable (kein "VCRUNTIME140.dll fehlt")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Kickforge VERSION 0.8.0 LANGUAGES C CXX)

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

# Quellen des Plugins (auch von den Tests mitbenutzt)
set(KICKFORGE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/KickforgeLookAndFeel.cpp
    Source/KickforgeLookAndFeel.h
    Source/IndustrialEngine.cpp
    Source/IndustrialEngine.h
    Source/SampleLibrary.cpp
    Source/SampleLibrary.h
    Source/ParamTable.h
    Source/FactoryPresets.h
    Source/Presets.h
    Source/KickVisualizer.h
    Source/Theme.h
)
target_sources(Kickforge PRIVATE ${KICKFORGE_SOURCES})

target_compile_options(Kickforge PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(Kickforge PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING>)

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
    # Engine- + Processor-Tests (linkt die Plugin-Quellen direkt, ohne Plugin-Wrapper)
    juce_add_console_app(KickforgeTests PRODUCT_NAME "KickforgeTests")
    target_sources(KickforgeTests PRIVATE Tests/PluginTests.cpp ${KICKFORGE_SOURCES})
    target_compile_definitions(KickforgeTests PRIVATE
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1
        $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING>
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JucePlugin_Name="Kickforge")
    target_compile_options(KickforgeTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(KickforgeTests PRIVATE
        KickforgeAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
    )

    # Render-Werkzeug für Klangvergleiche (IndustrialEngine ohne DAW)
    juce_add_console_app(KickforgeRender PRODUCT_NAME "KickforgeRender")
    target_sources(KickforgeRender PRIVATE Tests/RenderTool.cpp Source/IndustrialEngine.cpp)
    target_compile_definitions(KickforgeRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
        $<$<CXX_COMPILER_ID:MSVC>:_SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING>)
    target_compile_options(KickforgeRender PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(KickforgeRender PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
endif()
