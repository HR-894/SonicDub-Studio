# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║                         VideoDubber — CMake Build                          ║
# ║                                                                            ║
# ║  Industry-grade C++20 automated video dubbing application.                 ║
# ║                                                                            ║
# ║  Build System Architecture:                                                ║
# ║  ┌──────────────────────────────────────────────────────────────────────┐   ║
# ║  │  CMake 3.25+ ──► vcpkg (dependency manager)                        │   ║
# ║  │       │                                                             │   ║
# ║  │       ├── Qt6 (GUI framework)                                       │   ║
# ║  │       ├── FFmpeg (libav* — audio/video processing)                  │   ║
# ║  │       ├── whisper.cpp (local speech-to-text via GGML)               │   ║
# ║  │       ├── libcurl (HTTP client for API calls)                       │   ║
# ║  │       ├── Boost.Beast (local REST API server)                       │   ║
# ║  │       ├── spdlog (structured logging)                               │   ║
# ║  │       ├── nlohmann/json (JSON parsing)                              │   ║
# ║  │       └── GoogleTest (unit testing framework)                       │   ║
# ║  └──────────────────────────────────────────────────────────────────────┘   ║
# ║                                                                            ║
# ║  Quickstart:                                                               ║
# ║    cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/.../vcpkg.cmake       ║
# ║    cmake --build build --config Release                                    ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

cmake_minimum_required(VERSION 3.25)

project(VideoDubber
    VERSION     1.0.0
    LANGUAGES   CXX
    DESCRIPTION "Industry-grade C++ automated video dubbing application"
)

# ─── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # Enforce ISO C++20, no GNU extensions

# ─── vcpkg Integration ──────────────────────────────────────────────────────
# Auto-detect vcpkg if VCPKG_ROOT env var set and no toolchain specified
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# ─── Build Options ──────────────────────────────────────────────────────────
option(ENABLE_CUDA     "Enable CUDA acceleration for whisper.cpp" ON)
option(BUILD_TESTS     "Build unit tests (GoogleTest)"            ON)
option(BUILD_INSTALLER "Build NSIS installer target"              OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 1:  Find External Packages
# ═══════════════════════════════════════════════════════════════════════════════

# --- Qt6: GUI framework ---
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# --- libcurl: HTTP client for REST API calls ---
find_package(CURL REQUIRED)

# --- nlohmann/json: JSON reading/writing ---
find_package(nlohmann_json CONFIG REQUIRED)

# --- spdlog: Fast structured logging ---
find_package(spdlog CONFIG REQUIRED)

# --- Boost: Beast (HTTP server), Asio (async I/O) ---
find_package(Boost REQUIRED COMPONENTS system)

# --- Qt6 MOC/UIC/RCC automation ---
qt6_standard_project_setup()

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 2:  FFmpeg Libraries (C ABI)
# ═══════════════════════════════════════════════════════════════════════════════
# FFmpeg is a pure-C library, so we locate headers and libs manually.
# Required libs:
#   avcodec   — codec encode/decode
#   avformat  — container mux/demux
#   avutil    — utilities, pixel formats, error handling
#   avfilter  — audio/video filter graphs (used by AudioSyncer)
#   swresample — audio resampling (used by AudioExtractor)

find_path(AVCODEC_INCLUDE_DIR    libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY     avcodec)
find_library(AVFORMAT_LIBRARY    avformat)
find_library(AVUTIL_LIBRARY      avutil)
find_library(AVFILTER_LIBRARY    avfilter)
find_library(SWRESAMPLE_LIBRARY  swresample)

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 3:  whisper.cpp (Local Speech-to-Text Engine)
# ═══════════════════════════════════════════════════════════════════════════════
# Strategy: Try vcpkg first; fallback to FetchContent from GitHub.
find_package(whisper CONFIG QUIET)
if(NOT whisper_FOUND)
    message(STATUS "[VideoDubber] whisper.cpp not found via vcpkg — using FetchContent")
    include(FetchContent)
    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
        GIT_TAG        v1.6.0
    )
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(ENABLE_CUDA)
        set(WHISPER_CUBLAS ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(whisper)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 4:  Source Files & Executable Target
# ═══════════════════════════════════════════════════════════════════════════════
#
# Directory layout (see README.md for full tree):
#   src/
#   ├── core/          — Pipeline engine, config, threading, error handling
#   ├── stages/        — Pipeline stage implementations
#   ├── backends/      — Pluggable translation & TTS adapters
#   │   ├── translation/
#   │   └── tts/
#   ├── network/       — HTTP client & REST API server
#   ├── gui/           — Qt6 widgets and windows
#   ├── utils/         — FFmpeg wrappers, file I/O, JSON helpers
#   └── main.cpp       — Application entry point
#

file(GLOB_RECURSE SOURCES
    "src/core/*.cpp"
    "src/stages/*.cpp"
    "src/backends/translation/*.cpp"
    "src/backends/tts/*.cpp"
    "src/network/*.cpp"
    "src/gui/*.cpp"
    "src/utils/*.cpp"
    "src/main.cpp"
)

qt6_add_executable(VideoDubber ${SOURCES})

# ─── Include Directories ────────────────────────────────────────────────────
target_include_directories(VideoDubber PRIVATE
    ${CMAKE_SOURCE_DIR}/src        # Project headers  (e.g. #include "core/segment.h")
    ${AVCODEC_INCLUDE_DIR}         # FFmpeg C headers
    ${CURL_INCLUDE_DIRS}           # libcurl headers
)

# ─── Link Libraries ─────────────────────────────────────────────────────────
target_link_libraries(VideoDubber PRIVATE
    # --- GUI ---
    Qt6::Core Qt6::Widgets Qt6::Network

    # --- Networking ---
    CURL::libcurl
    Boost::system                   # Boost.Beast depends on Boost.System

    # --- Data Formats ---
    nlohmann_json::nlohmann_json

    # --- Logging ---
    spdlog::spdlog

    # --- Audio / Video ---
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SWRESAMPLE_LIBRARY}

    # --- Speech Recognition ---
    whisper
)

# ─── Compiler Definitions ───────────────────────────────────────────────────
target_compile_definitions(VideoDubber PRIVATE
    QT_NO_KEYWORDS                  # Avoid Qt macro conflicts (signals/slots → Q_SIGNALS/Q_SLOTS)
    NOMINMAX                        # Prevent Windows.h min/max macro pollution
    WIN32_LEAN_AND_MEAN             # Exclude rarely-used Windows headers
    $<$<BOOL:${ENABLE_CUDA}>:WHISPER_USE_CUDA>
)

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 5:  Unit Tests (GoogleTest)
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 6:  Platform-Specific Settings
# ═══════════════════════════════════════════════════════════════════════════════
if(WIN32)
    # WIN32_EXECUTABLE hides the console window in Release builds
    set_target_properties(VideoDubber PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
