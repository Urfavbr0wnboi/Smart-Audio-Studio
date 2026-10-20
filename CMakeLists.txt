cmake_minimum_required(VERSION 3.22)

project(SmartAudioStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAS_BUILD_TESTS "Build the unit test runner" ON)

# JUCE is fetched at configure time. To build against a local checkout instead, pass
# -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Engine sources have no GUI or audio-device dependencies, so the test runner
# can compile and exercise them without a sound card.
set(SAS_ENGINE_SOURCES
    Source/Engine/AudioFileLoader.cpp
    Source/Engine/MixEngine.cpp
    Source/Engine/Project.cpp
    Source/Engine/ProjectFile.cpp
    Source/Engine/Recorder.cpp
    Source/Engine/Waveform.cpp)

set(SAS_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
juce_add_gui_app(SmartAudioStudio
    PRODUCT_NAME "Smart Audio Studio"
    COMPANY_NAME "Smart Audio Studio"
    BUNDLE_ID    com.smartaudiostudio.app
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Smart Audio Studio needs microphone access to record audio.")

target_sources(SmartAudioStudio PRIVATE
    ${SAS_ENGINE_SOURCES}
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/ProjectDocument.cpp
    Source/Engine/AudioEngine.cpp
    Source/UI/BrowserPanel.cpp
    Source/UI/ChannelStrip.cpp
    Source/UI/LevelMeter.cpp
    Source/UI/MixerView.cpp
    Source/UI/SettingsOverlay.cpp
    Source/UI/StudioLookAndFeel.cpp
    Source/UI/TimelineView.cpp
    Source/UI/TransportBar.cpp)

target_include_directories(SmartAudioStudio PRIVATE Source)

target_compile_definitions(SmartAudioStudio PRIVATE
    ${SAS_COMMON_DEFINITIONS}
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SmartAudioStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SmartAudioStudio,JUCE_VERSION>")

target_link_libraries(SmartAudioStudio
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if (SAS_BUILD_TESTS)
    juce_add_console_app(SmartAudioStudioTests PRODUCT_NAME "Smart Audio Studio Tests")

    target_sources(SmartAudioStudioTests PRIVATE
        ${SAS_ENGINE_SOURCES}
        Tests/TestMain.cpp
        Tests/AudioFileLoaderTests.cpp
        Tests/MixEngineTests.cpp
        Tests/ProjectFileTests.cpp
        Tests/ProjectTests.cpp
        Tests/RecorderTests.cpp
        Tests/WaveformTests.cpp)

    target_include_directories(SmartAudioStudioTests PRIVATE Source)
    target_compile_definitions(SmartAudioStudioTests PRIVATE ${SAS_COMMON_DEFINITIONS})

    target_link_libraries(SmartAudioStudioTests
        PRIVATE
            juce::juce_audio_formats
            juce::juce_events
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME SmartAudioStudioTests COMMAND SmartAudioStudioTests)
endif()
