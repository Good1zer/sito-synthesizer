cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PamplejuceVersion)

# Modern concise way to add dependencies to your project
include(CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# =========================
# Project Info
# =========================
set(PROJECT_NAME "SITO")
set(PRODUCT_NAME "SITO")
set(COMPANY_NAME "u95")
set(BUNDLE_ID "com.u95.sito")
set(FORMATS VST3 Standalone CLAP)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

include(PamplejuceLog)

# =========================
# Dependencies
# =========================
add_subdirectory(JUCE)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
add_subdirectory(modules/melatonin_inspector)

# =========================
# Plugin Target
# =========================
juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE U95A
    PLUGIN_CODE Sito
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}"
)

clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES instrument
)

# =========================
# Shared Code
# =========================
add_library(SharedCode INTERFACE)
include(SharedCodeDefaults)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
)

target_sources(SharedCode INTERFACE ${SourceFiles})

target_include_directories(SharedCode
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# =========================
# Assets
# =========================
include(Assets)

# =========================
# Compile Definitions
# =========================
target_compile_definitions(SharedCode
    INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    SITO_ENABLE_INSPECTOR=$<IF:$<CONFIG:Debug>,1,0>
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="SITO"
)

# =========================
# Linking
# =========================
target_link_libraries(SharedCode
    INTERFACE
    Assets
    $<$<CONFIG:Debug>:melatonin_inspector>
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# =========================
# Platform / Extras
# =========================
include(PamplejuceIPP)
include(XcodePrettify)

# =========================
# IDE Grouping
# =========================
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/source FILES ${SourceFiles})

# =========================
# Tests & Benchmarks
# =========================
# Uncomment to enable tests and benchmarks
# include(Tests)
# include(Benchmarks)

# =========================
# CI Output
# =========================
include(GitHubENV)
