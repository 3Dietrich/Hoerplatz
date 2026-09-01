cmake_minimum_required(VERSION 3.22)

project(Hoerplatz VERSION 0.1.0)

# Ohne ausdrueckliche Bauart erzeugen Makefile-/Ninja-Generatoren einen Bau
# ganz ohne Optimierung; JUCEs Empfehlungen haengen an $<CONFIG:Release>.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Bauart" FORCE)
endif()

# JUCE liegt als Klon daneben und wird eingebunden, nicht kopiert.
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "$ENV{HOME}/Documents/JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(Hoerplatz
    COMPANY_NAME               "Dietrich Pank"
    BUNDLE_ID                  "de.pankd.hoerplatz"
    PLUGIN_MANUFACTURER_CODE   Dpnk
    PLUGIN_CODE                Hp01
    FORMATS                    AU VST3 Standalone
    PRODUCT_NAME               "Hoerplatz"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)

target_sources(Hoerplatz
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Params.cpp
        Source/UI/RoomComponent.cpp
        Source/UI/HeadSymbol.cpp)

target_compile_features(Hoerplatz PRIVATE cxx_std_17)

target_compile_definitions(Hoerplatz
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Hoerplatz
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Pruefung der reinen Rechnung - braucht weder JUCE noch eine Audioschnitt-
# stelle und laeuft deshalb auch auf einem Bauserver durch.
enable_testing()

add_executable(geometry_check Tests/geometry_check.cpp)
target_compile_features(geometry_check PRIVATE cxx_std_17)
add_test(NAME geometry_check COMMAND geometry_check)

# Zeichnet die Oberflaeche offscreen in eine PNG-Datei - zum Pruefen des
# Layouts, ohne dass auf dem Bildschirm ein Fenster erscheint.
juce_add_console_app(ui_shot PRODUCT_NAME "ui_shot")
target_sources(ui_shot PRIVATE
    Tests/ui_shot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/UI/RoomComponent.cpp
    Source/UI/HeadSymbol.cpp)
target_compile_features(ui_shot PRIVATE cxx_std_17)
target_compile_definitions(ui_shot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="Hoerplatz"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0)
target_link_libraries(ui_shot PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Nachweis am Signal: Impulsantwort durch den fertigen Prozessor.
juce_add_console_app(audio_check PRODUCT_NAME "audio_check")
target_sources(audio_check PRIVATE
    Tests/audio_check.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/UI/RoomComponent.cpp
    Source/UI/HeadSymbol.cpp)
target_compile_features(audio_check PRIVATE cxx_std_17)
target_compile_definitions(audio_check PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Name="Hoerplatz"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0)
target_link_libraries(audio_check PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME audio_check COMMAND audio_check)
