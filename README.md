# 🎬 SonicDub Studio

**Industry-grade C++20 automated video dubbing application for Windows.**

Auto-dubs any video from any language into Hindi (or any target language)
using local whisper.cpp transcription and cloud AI translation/TTS.

---

## 📁 Project Structure

```
SonicDubStudio/
│
├── CMakeLists.txt              ← Root build system (CMake 3.25+)
├── vcpkg.json                  ← Dependency manifest (vcpkg)
├── config.json                 ← Default configuration (copied to %APPDATA%)
├── README.md                   ← This file
│
├── src/                        ← All C++ source code
│   ├── main.cpp                ← Application entry point
│   │
│   ├── core/                   ← 🧠 Core Architecture
│   │   ├── segment.h           ← Central data structure (Segment struct)
│   │   ├── pipeline_manager.h  ← 7-stage pipeline orchestrator
│   │   ├── pipeline_manager.cpp
│   │   ├── config_manager.h    ← Thread-safe JSON config (singleton)
│   │   ├── config_manager.cpp
│   │   ├── thread_pool.h       ← Producer-consumer thread pool (C++20)
│   │   ├── thread_pool.cpp
│   │   ├── logger.h            ← Multi-sink spdlog wrapper
│   │   ├── logger.cpp
│   │   ├── error_handler.h     ← Exception hierarchy (VDException tree)
│   │   └── error_handler.cpp
│   │
│   ├── stages/                 ← 🔧 Pipeline Stage Implementations
│   │   ├── audio_extractor.h   ← Stage 1: Video → 16kHz WAV (FFmpeg)
│   │   ├── audio_extractor.cpp
│   │   ├── transcriber.h       ← Stage 2: WAV → Text (whisper.cpp)
│   │   ├── transcriber.cpp
│   │   ├── translator.h        ← Stage 3: ITranslator interface
│   │   ├── tts_engine.h        ← Stage 4: ITTSEngine interface
│   │   ├── audio_syncer.h      ← Stage 5: Time-stretch (FFmpeg atempo)
│   │   ├── audio_syncer.cpp
│   │   ├── audio_mixer.h       ← Stage 6: PCM timeline mixing
│   │   ├── audio_mixer.cpp
│   │   ├── video_muxer.h       ← Stage 7: Audio + Video → MP4
│   │   └── video_muxer.cpp
│   │
│   ├── backends/               ← 🔌 Pluggable Backend Adapters
│   │   ├── translation/
│   │   │   ├── google_translate.h/cpp    ← Google Translate (free + paid)
│   │   │   ├── deepl.h/cpp              ← DeepL Pro/Free API
│   │   │   └── libretranslate.h/cpp     ← Self-hosted open source
│   │   └── tts/
│   │       ├── gemini_tts.h/cpp         ← Google Gemini (recommended)
│   │       ├── google_cloud_tts.h/cpp   ← Google Cloud TTS
│   │       ├── elevenlabs_tts.h/cpp     ← ElevenLabs (premium voices)
│   │       └── edge_tts.h/cpp           ← Microsoft Edge TTS (free)
│   │
│   ├── network/                ← 🌐 HTTP Client & REST Server
│   │   ├── http_client.h/cpp   ← libcurl wrapper (GET/POST)
│   │   └── rest_api_server.h/cpp ← Boost.Beast server (:7070)
│   │
│   ├── gui/                    ← 🖥️ Qt6 User Interface
│   │   ├── main_window.h/cpp         ← Primary window + pipeline controller
│   │   ├── drop_zone_widget.h/cpp    ← Drag-and-drop area
│   │   ├── language_selector.h/cpp   ← Source/target language picker
│   │   ├── progress_panel.h/cpp      ← Multi-stage progress bars
│   │   ├── settings_dialog.h/cpp     ← API keys + backend config
│   │   └── log_viewer.h/cpp          ← Color-coded live log display
│   │
│   └── utils/                  ← 🛠️ Utility Libraries
│       ├── ffmpeg_wrapper.h/cpp   ← RAII wrappers for FFmpeg C types
│       ├── file_utils.h/cpp       ← Temp paths, binary I/O
│       ├── json_utils.h           ← Safe JSON access helpers
│       └── audio_utils.h/cpp      ← Audio processing utilities
│
├── tests/                      ← 🧪 Unit Tests (GoogleTest)
│   ├── CMakeLists.txt
│   ├── test_pipeline.cpp       ← ThreadPool, Segment, Mock tests
│   └── test_audio_extractor.cpp ← WAV file format tests
│
├── chrome_extension/           ← 🔌 Chrome Extension (YouTube integration)
│   ├── manifest.json           ← Extension manifest v3
│   ├── content.js              ← YouTube page injector
│   ├── popup.html/js           ← Extension popup UI
│   └── background.js           ← Service worker
│
└── installer/                  ← 📦 NSIS Windows Installer
    └── installer.nsi           ← Installer build script
```

---

## 🏗️ Architecture

### Pipeline Data Flow
```
Video File / URL
      │
      ▼
┌─────────────┐   16kHz    ┌──────────────┐  Segments  ┌────────────┐
│   Stage 1   │───mono────►│    Stage 2   │──────────►│   Stage 3  │
│  Extraction │    WAV     │ Transcription │  + text   │ Translation│
│  (FFmpeg)   │            │ (whisper.cpp) │           │ (parallel) │
└─────────────┘            └──────────────┘           └──────┬─────┘
                                                              │
      ┌──────────────────────────────────────────────────────┘
      ▼
┌─────────────┐  WAV files  ┌──────────┐  Timeline  ┌──────────┐
│   Stage 4   │────────────►│ Stage 5  │───────────►│ Stage 6  │
│     TTS     │             │   Sync   │            │   Mix    │
│ (parallel)  │             │ (atempo) │            │  (PCM)   │
└─────────────┘             └──────────┘            └────┬─────┘
                                                          │
                                                          ▼
                                    ┌─────────────────────────────┐
                                    │          Stage 7            │
                                    │      Video Muxer            │
                                    │  (FFmpeg: video + audio)    │
                                    └──────────┬──────────────────┘
                                               │
                                               ▼
                                        Final Dubbed MP4
```

### Threading Model
```
┌─────────────────────────────────────────────────────┐
│              PipelineManager                         │
│                                                      │
│  main_pool_ (2 threads)                              │
│  ├── Runs pipeline stages sequentially               │
│  │                                                    │
│  translate_pool_ (8 threads)  ◄── Stage 3            │
│  ├── Parallel API calls                              │
│  │                                                    │
│  tts_pool_ (4 threads)  ◄── Stage 4                  │
│  └── Parallel API calls                              │
│                                                      │
│  Qt GUI Thread  ◄── MainWindow, signals/slots         │
│  REST API Thread  ◄── Boost.Beast server              │
└─────────────────────────────────────────────────────┘
```

### Design Patterns Used

| Pattern          | Where                              | Why                                    |
|------------------|------------------------------------|----------------------------------------|
| **Singleton**    | ConfigManager, Logger              | Global access to shared services       |
| **Strategy**     | ITranslator, ITTSEngine            | Pluggable backends without pipeline changes |
| **Factory**      | make_translator(), make_tts()      | Create backends from config strings    |
| **Observer**     | ProgressCallback                   | Decouple pipeline from GUI updates     |
| **RAII**         | FFmpeg wrappers (unique_ptr)       | Exception-safe C resource management   |
| **Pimpl**        | Transcriber, HttpClient, REST server | Hide heavy dependencies from headers  |
| **Producer-Consumer** | ThreadPool                   | Work queue with condition variables    |

### Data Structures & Complexity

| Structure                     | DSA Type                       | Complexity         |
|-------------------------------|--------------------------------|--------------------|
| `Segment`                     | Struct (POD-like data carrier) | —                  |
| `SegmentList`                 | `std::vector<Segment>`         | O(1) access by ID  |
| `ThreadPool::tasks_`          | `std::queue<function>`         | O(1) push/pop FIFO |
| `PipelineManager::jobs_`      | `std::unordered_map<string, ptr>` | O(1) avg lookup |
| `PipelineManager::statuses_`  | `std::unordered_map<string, status>` | O(1) avg lookup |
| Audio PCM buffer              | `std::vector<int16_t>`         | O(1) amortized append |
| Config store                  | `nlohmann::json` (ordered_map) | O(log n) key lookup |

---

## 🚀 Quick Start

### Prerequisites
- Windows 10/11 (x64)
- Visual Studio 2022 (C++ workload)
- CMake 3.25+
- vcpkg
- FFmpeg in PATH
- Python 3.x (for edge-tts only)

### 1. Setup vcpkg
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat && cd ..
set VCPKG_ROOT=%CD%\vcpkg
```

### 2. Install Dependencies
```bash
vcpkg install qt6-base qt6-widgets curl nlohmann-json spdlog boost-beast boost-asio gtest
```

### 3. Download Whisper Model
```bash
mkdir %APPDATA%\SonicDubStudio\models
curl -L -o %APPDATA%\SonicDubStudio\models\ggml-medium.bin ^
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin
```

### 4. Build
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 5. Configure API Keys
Edit `config.json` or use the Settings dialog:
```json
{
  "api_keys": {
    "gemini": "your-key-here",
    "google_translate": "your-key-here"
  }
}
```

### 6. Run
```bash
build\Release\SonicDubStudio.exe
```

---

## 🌐 REST API (port 7070)

| Method | Endpoint                      | Description           |
|--------|-------------------------------|-----------------------|
| POST   | `/api/v1/dub`                 | Start dubbing job     |
| GET    | `/api/v1/job/{id}/status`     | Get job progress      |
| GET    | `/api/v1/job/{id}/download`   | Get output file path  |

---

## 🧪 Tests
```bash
cmake --build build --target vd_tests
.\build\tests\Release\vd_tests.exe
```

---

## 📦 Build Installer
```bash
makensis installer\installer.nsi
```

---

## 🗺️ Roadmap
- [ ] Speaker diarization (multi-voice dubbing)
- [ ] Lip sync via Wav2Lip
- [ ] Voice cloning (ElevenLabs)
- [ ] Offline translation via Ollama
- [ ] Batch queue UI
