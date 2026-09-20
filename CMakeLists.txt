cmake_minimum_required(VERSION 3.22)

project(DiamondRoom VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/MP)
endif()

add_subdirectory(JUCE)

# Photographic assets cropped from the reference mockup: the knob body (with
# its pointer inpainted out, since that is drawn separately and rotated per
# value), the two fader gems, and the title badge. Everything else on the
# panel is still generated procedurally.
juce_add_binary_data(DiamondRoomAssets SOURCES
    Assets/KnobBody.png
    Assets/FaderGemSection.png
    Assets/FaderGemMaster.png
    Assets/TitleBadge.png
)

juce_add_plugin(DiamondRoom
    COMPANY_NAME              "ZenoxArt"
    PLUGIN_MANUFACTURER_CODE  Znxa
    PLUGIN_CODE               Dmrm
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "Diamond Room"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_CATEGORIES           Fx Reverb
)

target_sources(DiamondRoom PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/PresetManager.cpp
    Source/dsp/OneKnobDriver.cpp
    Source/dsp/HReverb.cpp
    Source/dsp/MannyMReverb.cpp
    Source/dsp/ValhallaVerb.cpp
    Source/dsp/TrueVerb.cpp
    Source/dsp/CleanVoiceTube.cpp
    Source/gui/Theme.cpp
    Source/gui/MetalKnob.cpp
    Source/gui/MetalSlider.cpp
    Source/gui/PanelSection.cpp
    Source/gui/RackButton.cpp
)

target_compile_definitions(DiamondRoom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(DiamondRoom PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    DiamondRoomAssets
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

#==============================================================================
# Development helper: renders the editor to a PNG offscreen, so the panel
# artwork can be checked without loading the plugin into a host.
option(DIAMONDROOM_BUILD_SCREENSHOT "Build the offscreen editor renderer" OFF)

if (DIAMONDROOM_BUILD_SCREENSHOT)
    juce_add_console_app(DiamondRoomShot PRODUCT_NAME "DiamondRoomShot")
    target_sources(DiamondRoomShot PRIVATE Tools/RenderEditor.cpp)
    target_link_libraries(DiamondRoomShot PRIVATE
        DiamondRoom
        DiamondRoomAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    target_compile_definitions(DiamondRoomShot PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        # The self-tests have to let APVTS's timer run to see parameter
        # changes reach the value tree. Only the test tool needs this.
        JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
