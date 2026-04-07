/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  pipeline_manager.cpp — Pipeline Orchestrator (Implementation)            ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE EXECUTION FLOW:
 *
 *   run_pipeline(ctx) — called asynchronously from main_pool_
 *   ────────────────────────────────────────────────────────────
 *
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 1: EXTRACTION (5%)                                 │
 *   │  AudioExtractor: video → 16kHz mono WAV                   │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 2: TRANSCRIPTION (15%)                             │
 *   │  Transcriber: WAV → SegmentList (via whisper.cpp)         │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 3: TRANSLATION (30–50%) *** PARALLEL ***           │
 *   │  Fan-out N segments → translate_pool_ → fan-in            │
 *   │  Each segment: ITranslator::translate()                   │
 *   │  Retry logic: up to 3 attempts with exponential backoff   │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 4: TTS (50–70%) *** PARALLEL ***                   │
 *   │  Fan-out N segments → tts_pool_ → fan-in                  │
 *   │  Each segment: ITTSEngine::synthesize()                   │
 *   │  Retry logic: exponential backoff (2^attempt * base_ms)   │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 5: SYNC (70%)                                      │
 *   │  AudioSyncer: time-stretch each TTS audio to match        │
 *   │  original segment duration (FFmpeg atempo filter)          │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 6: MIXING (80%)                                    │
 *   │  AudioMixer: place all synced segments on timeline         │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  Stage 7: MUXING (90%)                                    │
 *   │  VideoMuxer: merge dubbed audio + original video           │
 *   └─────────────────────┬──────────────────────────────────────┘
 *                         ▼
 *                     CLEANUP (98%)
 *                     Delete temp dir
 *                         ▼
 *                      DONE (100%)
 *
 * ERROR HANDLING:
 *   Any exception in any stage → catch → mark job as "failed" → cleanup.
 *
 * CANCELLATION:
 *   ctx->cancelled is checked between stages.  If true, pipeline exits early.
 *
 * DESIGN PATTERNS:
 *   - Factory Method:  make_translator(), make_tts()
 *   - Fan-Out/Fan-In: Stages 3 & 4 submit N parallel tasks, wait for all
 *   - Observer:        emit_progress() → ProgressCallback
 */

#include "core/pipeline_manager.h"
#include "core/config_manager.h"
#include "core/logger.h"
#include "core/error_handler.h"

// ── Pipeline Stage Includes ──────────────────────────────────────────────────
#include "stages/audio_extractor.h"
#include "stages/transcriber.h"
#include "stages/diarizer.h"
#include "stages/voice_profiler.h"
#include "stages/audio_syncer.h"
#include "stages/audio_mixer.h"
#include "stages/video_muxer.h"

// ── Backend Factory Includes ─────────────────────────────────────────────────
#include "backends/translation/google_translate.h"
#include "backends/translation/libretranslate.h"
#include "backends/translation/deepl.h"
#include "backends/tts/google_cloud_tts.h"
#include "backends/tts/gemini_tts.h"
#include "backends/tts/elevenlabs_tts.h"
#include "backends/tts/edge_tts.h"
#include "backends/tts/xtts_engine.h"

// ── Standard Library ─────────────────────────────────────────────────────────
#include "utils/file_utils.h"

#include <QProcess>
#include <QThread>
#include <QCoreApplication>

#include <random>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// JobContext — Per-job internal state (not exposed to callers)
// ═══════════════════════════════════════════════════════════════════════════════

struct PipelineManager::JobContext {
    std::string               job_id;        ///< Unique hex identifier
    JobConfig                 config;        ///< User-provided settings
    SegmentList               segments;      ///< Segments flowing through pipeline
    std::string               temp_dir;      ///< Temporary working directory
    std::atomic<bool>         cancelled{false}; ///< Set by cancel_job()
    std::chrono::steady_clock::time_point start_time; ///< For ETA calculation
};

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor — Initialize thread pools
// ═══════════════════════════════════════════════════════════════════════════════

PipelineManager::PipelineManager() {
    auto& cfg = ConfigManager::instance();

    // main_pool_: runs one pipeline at a time (2 threads for overlap)
    main_pool_      = std::make_unique<ThreadPool>(2);

    // tts_pool_: parallel TTS API calls (bottleneck stage)
    tts_pool_       = std::make_unique<ThreadPool>(cfg.get_max_parallel_tts());

    // translate_pool_: parallel translation API calls
    translate_pool_ = std::make_unique<ThreadPool>(cfg.get_max_parallel_translate());

    // State persistence directory
    state_dir_ = cfg.get_temp_dir() + "/job_states";
    std::filesystem::create_directories(state_dir_);

    // ── Boot AI Bridge ──────────────────────────────────────────────────
    // STRATEGY: Tell Python to bind to port 0 (OS picks a free port).
    // Python writes the actual port to a temp file. C++ reads it.
    // This eliminates the TOCTOU race of probe-then-release-then-bind.
    ai_bridge_process_ = std::make_unique<QProcess>();

    // Pipe Python stderr → our log so crash tracebacks are visible
    ai_bridge_process_->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(ai_bridge_process_.get(), &QProcess::readyReadStandardOutput,
        [this]() {
            QByteArray data = ai_bridge_process_->readAllStandardOutput();
            VD_LOG_INFO("[AI Bridge] {}", data.toStdString());
        });

    // Port file: Python writes the port it bound to here
    std::string port_file = cfg.get_temp_dir() + "/bridge_port.txt";
    // Parent PID: Python watchdog kills itself if we crash
    auto parent_pid = QString::number(QCoreApplication::applicationPid());

    // Prefer PyInstaller bundle; fall back to raw Python for developers
    QString exe_path = "ai_engine/ai_bridge_server.exe";
    if (std::filesystem::exists(exe_path.toStdString())) {
        ai_bridge_process_->setProgram(exe_path);
        ai_bridge_process_->setArguments({
            "--port", "0",
            "--parent-pid", parent_pid,
            "--port-file", QString::fromStdString(port_file)
        });
    } else {
        ai_bridge_process_->setProgram("python");
        ai_bridge_process_->setArguments({
            "ai_engine/server.py",
            "--port", "0",
            "--parent-pid", parent_pid,
            "--port-file", QString::fromStdString(port_file)
        });
    }
    ai_bridge_process_->start();

    // Wait for Python to write the port file (up to 30 seconds)
    bool bridge_ready = false;
    for (int i = 0; i < 60; ++i) {
        QThread::msleep(500);
        if (std::filesystem::exists(port_file)) {
            std::ifstream pf(port_file);
            int port_val = 0;
            if (pf >> port_val && port_val > 0) {
                bridge_port_ = static_cast<uint16_t>(port_val);
                bridge_ready = true;
                break;
            }
        }
    }
    // Clean up the temp port file
    std::filesystem::remove(port_file);

    if (bridge_ready) {
        VD_LOG_INFO("PipelineManager: AI Bridge ready on port {}", bridge_port_);
    } else {
        VD_LOG_WARN("PipelineManager: AI Bridge did not start in time. "
                    "Diarization/Voice Cloning will be unavailable.");
    }
}

PipelineManager::~PipelineManager() {
    main_pool_->shutdown();
    tts_pool_->shutdown();
    translate_pool_->shutdown();
    if (ai_bridge_process_ && ai_bridge_process_->state() == QProcess::Running) {
        ai_bridge_process_->terminate();
        ai_bridge_process_->waitForFinished(3000);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// generate_job_id — Random 16-char hex string
// ═══════════════════════════════════════════════════════════════════════════════
// Collision probability: ~1 in 2^64 ≈ negligible

std::string PipelineManager::generate_job_id() const {
    // Static RNG + mutex: mt19937_64 is not thread-safe, but generate_job_id()
    // can be called from any thread via start_job().  Without this mutex,
    // concurrent calls cause data races on the RNG internal state.
    static std::mutex rng_mutex;
    static std::mt19937_64 rng(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint64_t> dist;
    std::lock_guard lock(rng_mutex);
    std::ostringstream oss;
    oss << std::hex << dist(rng);
    return oss.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
// start_job — Create job context and submit to main_pool_
// ═══════════════════════════════════════════════════════════════════════════════

std::string PipelineManager::start_job(const JobConfig& config) {
    auto ctx        = std::make_shared<JobContext>();
    ctx->job_id     = generate_job_id();
    ctx->config     = config;
    ctx->start_time = std::chrono::steady_clock::now();
    ctx->temp_dir   = ConfigManager::instance().get_temp_dir() + "/" + ctx->job_id;
    std::filesystem::create_directories(ctx->temp_dir);

    // Register in lookup maps
    {
        std::lock_guard lock(jobs_mutex_);
        jobs_[ctx->job_id]     = ctx;
        statuses_[ctx->job_id] = {ctx->job_id, "queued", 0};
    }

    VD_LOG_INFO("Job {} started: {}", ctx->job_id, config.input_path);

    // Submit pipeline to background thread
    main_pool_->submit([this, ctx]() { run_pipeline(ctx); });
    return ctx->job_id;
}

// ═══════════════════════════════════════════════════════════════════════════════
// emit_progress — Notify GUI / REST API of stage changes
// ═══════════════════════════════════════════════════════════════════════════════

void PipelineManager::emit_progress(const std::string& job_id,
                                     const std::string& stage,
                                     int progress, int eta) {
    std::lock_guard lock(jobs_mutex_);
    auto& s = statuses_[job_id];
    s.stage       = stage;
    s.progress    = progress;
    s.eta_seconds = eta;
    if (progress_cb_) progress_cb_(s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Factory Functions — Strategy Pattern for Backend Selection
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Creates a translator based on config string.
 * Supported: "google", "libretranslate", "deepl"
 */
static std::unique_ptr<ITranslator> make_translator(const std::string& backend) {
    if (backend == "google")          return std::make_unique<GoogleTranslate>();
    if (backend == "libretranslate")  return std::make_unique<LibreTranslate>();
    if (backend == "deepl")           return std::make_unique<DeepL>();
    throw ConfigException("Unknown translation backend: " + backend);
}

/**
 * Creates a TTS engine based on config string.
 * Supported: "google", "gemini", "elevenlabs", "edge"
 */
static std::unique_ptr<ITTSEngine> make_tts(const std::string& backend, uint16_t bridge_port = 0) {
    if (backend == "google")     return std::make_unique<GoogleCloudTTS>();
    if (backend == "gemini")     return std::make_unique<GeminiTTS>();
    if (backend == "elevenlabs") return std::make_unique<ElevenLabsTTS>();
    if (backend == "edge")       return std::make_unique<EdgeTTS>();
    if (backend == "xttsv2_local") return std::make_unique<XTTSBackend>(bridge_port);
    throw ConfigException("Unknown TTS backend: " + backend);
}

// ═══════════════════════════════════════════════════════════════════════════════
// run_pipeline — The Main 7-Stage Pipeline Execution
// ═══════════════════════════════════════════════════════════════════════════════

void PipelineManager::run_pipeline(std::shared_ptr<JobContext> ctx) {
    auto& cfg = ConfigManager::instance();

    try {

        // ─────────────────────────────────────────────────────────────────
        // STAGE 1: Audio Extraction  [5%]
        // Input:  video file (MP4/MKV/AVI) or URL
        // Output: 16kHz mono WAV file (optimized for whisper.cpp)
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "extraction", 5);
        VD_LOG_INFO("[{}] ▶ Stage 1: Audio Extraction", ctx->job_id);

        AudioExtractor extractor;
        std::string audio_path = ctx->temp_dir + "/audio_raw.wav";
        extractor.extract(ctx->config.input_path, audio_path);
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 2: Transcription  [15%]
        // Input:  WAV file
        // Output: SegmentList with timestamps + original_text
        // Engine: whisper.cpp (GGML) — runs on CPU or CUDA GPU
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "transcription", 15);
        VD_LOG_INFO("[{}] ▶ Stage 2: Transcription", ctx->job_id);

        Transcriber transcriber;
        ctx->segments = transcriber.transcribe(audio_path, ctx->config.source_language);
        VD_LOG_INFO("[{}]   Got {} segments", ctx->job_id, ctx->segments.size());
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 2.5: Diarization & Voice Profiling [20%]
        // Input:  16kHz WAV + SegmentList
        // Output: SegmentList with speaker_id, and speaker_refs Map
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "diarization", 20);
        VD_LOG_INFO("[{}] ▶ Stage 2b: Diarization", ctx->job_id);
        
        Diarizer diarizer(bridge_port_);
        diarizer.diarize(audio_path, ctx->segments);
        if (ctx->cancelled) return;

        VD_LOG_INFO("[{}] ▶ Stage 2c: Voice Profiling", ctx->job_id);
        VoiceProfiler profiler;
        auto speaker_refs = profiler.profile_speakers(ctx->config.input_path, ctx->temp_dir, ctx->segments);
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 3: Translation  [30–50%]  *** PARALLEL ***
        //
        // ALGORITHM: Fan-Out / Fan-In
        //   1. Create translator instance (factory pattern)
        //   2. For each segment, submit translate task to translate_pool_
        //   3. Each task has retry logic: 3 attempts, 500ms backoff
        //   4. Wait for all futures to complete (fan-in)
        //
        // DSA: vector<future<void>> — O(n) space for n segments
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "translation", 30);
        VD_LOG_INFO("[{}] ▶ Stage 3: Translation ({})", ctx->job_id,
                     ctx->config.translation_backend);

        auto translator = make_translator(ctx->config.translation_backend);
        std::vector<std::future<void>> translate_futures;
        std::atomic<size_t> translated_count{0};

        for (auto& seg : ctx->segments) {
            // FIX #3: Capture specific variables instead of illegal [&, &seg].
            //         The C++ standard forbids explicitly capturing a variable
            //         by-ref when the default capture is already '&'.
            // FIX #7: Check ctx->cancelled at the start of each task so that
            //         queued-but-not-yet-started tasks exit immediately on cancel,
            //         instead of burning API credits for every segment.
            translate_futures.push_back(translate_pool_->submit(
                [&seg, &translator, &translated_count, ctx, this]() {
                // Early exit if job was cancelled while this task was queued
                if (ctx->cancelled) return;

                int retry = 0;
                while (retry < 3) {
                    try {
                        seg.translated_text = translator->translate(
                            seg.original_text,
                            seg.source_language.empty() ? "en" : seg.source_language,
                            ctx->config.target_language);
                        seg.state = Segment::State::Translated;
                        break;  // Success
                    } catch (const std::exception& e) {
                        ++retry;
                        VD_LOG_WARN("[{}]   Translation retry {}: {}",
                                     ctx->job_id, retry, e.what());
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(500 * retry));
                    }
                }

                // FIX #5: Capture the incremented value atomically in one expression.
                // Without this, two threads could increment then read in opposite order,
                // causing the progress bar to jump backwards (e.g. 36% then 35%).
                size_t current = ++translated_count;
                int pct = 30 + static_cast<int>(
                    20.0 * current / ctx->segments.size());
                emit_progress(ctx->job_id, "translation", pct);
            }));
        }

        // Fan-in: wait for all translation tasks
        for (auto& f : translate_futures) f.get();
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 4: Text-to-Speech  [50–70%]  *** PARALLEL ***
        //
        // ALGORITHM: Fan-Out / Fan-In with Exponential Backoff
        //   retry_delay = base_ms * 2^attempt  (1s, 2s, 4s, ...)
        //
        // DSA: Each TTS result stored in seg.tts_audio_data (vector<uint8_t>)
        //      Then written to temp file for AudioSyncer input.
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "tts", 50);
        VD_LOG_INFO("[{}] ▶ Stage 4: TTS ({})", ctx->job_id, ctx->config.tts_backend);

        auto tts            = make_tts(ctx->config.tts_backend, bridge_port_);
        // Tell XTTS backend where to write output so files land in temp_dir
        if (auto* xtts = dynamic_cast<XTTSBackend*>(tts.get())) {
            xtts->set_output_dir(ctx->temp_dir);
        }
        int  retry_count    = cfg.get_tts_retry_count();
        int  retry_delay_ms = cfg.get_tts_retry_delay_ms();
        std::atomic<size_t> tts_count{0};
        std::vector<std::future<void>> tts_futures;

        for (auto& seg : ctx->segments) {
            std::string voice_id = cfg.get_tts_voice_id();
            std::string ref_path = "";
            if (ctx->config.tts_backend == "xttsv2_local" && speaker_refs.count(seg.speaker_id)) {
                ref_path = speaker_refs.at(seg.speaker_id);
            }

            tts_futures.push_back(tts_pool_->submit(
                [&seg, &tts, &tts_count, retry_count, retry_delay_ms, voice_id, ref_path, ctx, this]() {
                // FIX #7: Skip this task entirely if already cancelled
                if (ctx->cancelled) return;

                // Set segment_id on XTTS so Python generates a unique filename
                if (auto* xtts = dynamic_cast<XTTSBackend*>(tts.get())) {
                    xtts->set_segment_id(seg.id);
                }

                for (int attempt = 0; attempt < retry_count; ++attempt) {
                    try {
                        // Call TTS API
                        if (!ref_path.empty()) {
                            seg.tts_audio_data = tts->synthesize(seg.translated_text,
                                                                 ctx->config.target_language,
                                                                 ref_path);
                        } else {
                            seg.tts_audio_data = tts->synthesize(seg.translated_text,
                                                                 ctx->config.target_language,
                                                                 voice_id);
                        }

                        // Write audio data to temp file
                        seg.tts_audio_path = ctx->temp_dir
                            + "/tts_" + std::to_string(seg.id) + ".wav";
                        std::ofstream f(seg.tts_audio_path, std::ios::binary);
                        f.write(reinterpret_cast<const char*>(seg.tts_audio_data.data()),
                                seg.tts_audio_data.size());
                        seg.state = Segment::State::TTSDone;
                        break;  // Success

                    } catch (const TTSException& e) {
                        VD_LOG_WARN("[{}]   TTS attempt {}/{}: {}",
                                     ctx->job_id, attempt + 1, retry_count, e.what());
                        if (attempt + 1 < retry_count) {
                            // Exponential backoff: 1s, 2s, 4s, ...
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(
                                    retry_delay_ms * (1 << attempt)));
                        } else {
                            VD_LOG_ERROR("[{}]   TTS failed for segment {}, skipping",
                                          ctx->job_id, seg.id);
                            seg.state = Segment::State::Failed;
                        }
                    }
                }
                // FIX #5: Monotonic progress via atomic capture
                size_t current = ++tts_count;
                int pct = 50 + static_cast<int>(
                    20.0 * current / ctx->segments.size());
                emit_progress(ctx->job_id, "tts", pct);
            }));
        }

        // Fan-in: wait for all TTS tasks
        for (auto& f : tts_futures) f.get();
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 5: Audio Sync / Time-Stretch  [70%]
        // Adjusts each TTS segment duration to match original timing
        // Uses FFmpeg atempo filter (valid range: 0.5–100.0 per filter)
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "sync", 70);
        VD_LOG_INFO("[{}] ▶ Stage 5: Audio Sync", ctx->job_id);

        AudioSyncer syncer;
        for (auto& seg : ctx->segments) {
            if (seg.state == Segment::State::Failed) continue;  // Skip failed

            seg.synced_audio_path = ctx->temp_dir
                + "/synced_" + std::to_string(seg.id) + ".wav";
            syncer.sync(seg.tts_audio_path, seg.synced_audio_path, seg.duration_ms());
            seg.state = Segment::State::Synced;
        }
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 6: Audio Mixing  [80%]
        // Places all synced segments onto a single timeline
        // Uses PCM-level mixing with clipping prevention
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "mixing", 80);
        VD_LOG_INFO("[{}] ▶ Stage 6: Audio Mixing", ctx->job_id);

        AudioMixer mixer;
        std::string mixed_audio = ctx->temp_dir + "/dubbed_audio.wav";
        mixer.mix(ctx->segments, mixed_audio);
        if (ctx->cancelled) return;

        // ─────────────────────────────────────────────────────────────────
        // STAGE 7: Video Muxing  [90%]
        // Combines original video + dubbed audio into final output
        // Mode "overlay": original audio lowered + dubbed overlaid
        // Mode "replace": original audio completely replaced
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "muxing", 90);
        VD_LOG_INFO("[{}] ▶ Stage 7: Video Muxing", ctx->job_id);

        VideoMuxer muxer;
        muxer.mux(ctx->config.input_path,
                  mixed_audio,
                  ctx->config.output_path,
                  ctx->config.audio_mix_mode,
                  cfg.get_original_audio_vol());

        // ─────────────────────────────────────────────────────────────────
        // CLEANUP  [98%]
        // ─────────────────────────────────────────────────────────────────
        emit_progress(ctx->job_id, "cleanup", 98);
        std::filesystem::remove_all(ctx->temp_dir);

        // ─────────────────────────────────────────────────────────────────
        // DONE  [100%]
        // ─────────────────────────────────────────────────────────────────
        {
            std::lock_guard lock(jobs_mutex_);
            auto& s = statuses_[ctx->job_id];
            s.completed   = true;
            s.progress    = 100;
            s.stage       = "done";
            s.output_path = ctx->config.output_path;
            if (progress_cb_) progress_cb_(s);
        }
        VD_LOG_INFO("[{}] ✅ Job completed: {}", ctx->job_id, ctx->config.output_path);

    } catch (const std::exception& e) {
        // ── Any unhandled exception → mark job as failed ──────────────
        VD_LOG_ERROR("[{}] ❌ Pipeline failed: {}", ctx->job_id, e.what());

        std::lock_guard lock(jobs_mutex_);
        auto& s = statuses_[ctx->job_id];
        s.failed        = true;
        s.error_message = e.what();
        s.stage         = "failed";
        if (progress_cb_) progress_cb_(s);

        // Cleanup on failure too
        std::error_code ec;
        std::filesystem::remove_all(ctx->temp_dir, ec);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// cancel_job — Signal cancellation (checked between stages)
// ═══════════════════════════════════════════════════════════════════════════════

void PipelineManager::cancel_job(const std::string& job_id) {
    std::lock_guard lock(jobs_mutex_);
    auto it = jobs_.find(job_id);
    if (it != jobs_.end()) {
        it->second->cancelled = true;
        VD_LOG_INFO("Job {} cancelled by user", job_id);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// get_status — O(1) lookup in hash map
// ═══════════════════════════════════════════════════════════════════════════════

JobStatus PipelineManager::get_status(const std::string& job_id) const {
    std::lock_guard lock(jobs_mutex_);
    auto it = statuses_.find(job_id);
    if (it == statuses_.end()) return {job_id, "unknown", 0};
    return it->second;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Misc
// ═══════════════════════════════════════════════════════════════════════════════

void PipelineManager::set_progress_callback(ProgressCallback cb) {
    progress_cb_ = std::move(cb);
}

std::vector<std::string> PipelineManager::list_jobs() const {
    std::lock_guard lock(jobs_mutex_);
    std::vector<std::string> ids;
    ids.reserve(jobs_.size());
    for (const auto& [id, _] : jobs_) ids.push_back(id);
    return ids;
}

void PipelineManager::save_state(const std::string& /*job_id*/) const {
    // TODO: Serialize JobContext to JSON for crash recovery
}

bool PipelineManager::restore_job(const std::string& /*job_id*/) {
    // TODO: Deserialize and resume from last completed stage
    return false;
}

} // namespace vd
