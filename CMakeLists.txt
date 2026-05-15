cmake_minimum_required(VERSION 3.22)
project(InstaWidth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

juce_add_plugin(InstaWidth
    COMPANY_NAME "InstaWidth"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Inst
    PLUGIN_CODE Iwid
    FORMATS VST3 AU LV2
    LV2URI "https://github.com/hariel1985/InstaWidth"
    PRODUCT_NAME "InstaWidth"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(InstaWidth)

juce_add_binary_data(InstaWidthData SOURCES
    Resources/Rajdhani-Regular.ttf
    Resources/Rajdhani-Medium.ttf
    Resources/Rajdhani-Bold.ttf
)

target_sources(InstaWidth
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LookAndFeel.cpp
        Source/WidthEngine.cpp
        Source/SideDeEsser.cpp
        Source/SideFIRBuilder.cpp
        Source/Goniometer.cpp
        Source/CorrelationMeter.cpp
        Source/BandedCorrelation.cpp
)

target_compile_definitions(InstaWidth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(InstaWidth
    PRIVATE
        InstaWidthData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
