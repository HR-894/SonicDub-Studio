/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  pipeline_manager.h — Pipeline Orchestrator                               ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   The PipelineManager is the central brain of VideoDubber. It receives
 *   a JobConfig (input path + settings), runs the 7-stage pipeline, and
 *   reports progress back to the GUI / REST API.
 *
 * PIPELINE STAGES:
 *
 *   Stage │  Name         │  Worker Pool       │  Description
 *   ──────┼───────────────┼────────────────────┼─────────────────────────────
 *     1   │ Extraction    │  main_pool_ (1)    │  FFmpeg: Video → WAV
 *     2   │ Transcription │  main_pool_ (1)    │  whisper.cpp: WAV → Segments
 *     3   │ Translation   │  translate_pool_   │  Google/DeepL → translated text
 *     4   │ TTS           │  tts_pool_         │  Gemini/ElevenLabs → audio
 *     5   │ Sync          │  main_pool_ (1)    │  FFmpeg atempo → time-stretch
 *     6   │ Mix           │  main_pool_ (1)    │  PCM mixing → dubbed track
 *     7   │ Mux           │  main_pool_ (1)    │  FFmpeg: merge audio + video
 *
 *   Stages 3 & 4 are the bottleneck (API calls), so they use dedicated
 *   thread pools for parallelism (fan-out / fan-in pattern).
 *
 * JOB LIFECYCLE:
 *   start_job(config) → returns job_id → pipeline runs async → done/failed
 *
 * DSA NOTES:
 *   - Jobs:    std::unordered_map<job_id, JobContext>  — O(1) lookup
 *   - Status:  std::unordered_map<job_id, JobStatus>   — O(1) lookup
 *   - Job IDs: Random 64-bit hex (collision probability negligible)
 *
 * DESIGN PATTERNS:
 *   - Strategy: ITranslator / ITTSEngine interfaces  (pluggable backends)
 *   - Factory:  make_translator() / make_tts()       (backend construction)
 *   - Observer: ProgressCallback                      (GUI notifications)
 *
 * DEPENDENCIES:
 *   segment.h, thread_pool.h, <nlohmann/json>, <unordered_map>
 */

#pragma once

#include "core/segment.h"
#include "core/thread_pool.h"

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// JobConfig — Input parameters for a dubbing job
// ═══════════════════════════════════════════════════════════════════════════════

struct JobConfig {
    std::string input_path;          ///< Local file path or YouTube URL
    std::string source_language;     ///< "auto" for detection, or ISO code
    std::string target_language;     ///< ISO 639-1 code (e.g. "hi", "es")
    std::string tts_backend;         ///< "gemini", "google", "elevenlabs", "edge"
    std::string translation_backend; ///< "google", "deepl", "libretranslate"
    std::string output_path;         ///< Where to save final video
    std::string audio_mix_mode;      ///< "replace" or "overlay"
};

// ═══════════════════════════════════════════════════════════════════════════════
// JobStatus — Real-time status of a running/completed job
// ═══════════════════════════════════════════════════════════════════════════════

struct JobStatus {
    std::string job_id;              ///< Unique hex identifier
    std::string stage;               ///< Current stage name
    int         progress{0};         ///< 0–100 overall completion
    int         eta_seconds{-1};     ///< Estimated time remaining (-1 = unknown)
    bool        completed{false};    ///< true when all stages finished
    bool        failed{false};       ///< true if an error occurred
    std::string error_message;       ///< Description if failed == true
    std::string output_path;         ///< Path to output file when completed
};

/// Callback type for progress notifications (GUI, REST API, etc.)
using ProgressCallback = std::function<void(const JobStatus&)>;

// ═══════════════════════════════════════════════════════════════════════════════
// PipelineManager — The Central Pipeline Orchestrator
// ═══════════════════════════════════════════════════════════════════════════════

class PipelineManager {
public:
    explicit PipelineManager();
    ~PipelineManager();

    // ─── Job Control ──────────────────────────────────────────────────────

    /// Start a new dubbing job.  Returns unique job_id.  Runs async.
    std::string start_job(const JobConfig& config);

    /// Cancel a running job (sets cancelled flag, checked between stages).
    void cancel_job(const std::string& job_id);

    /// Get current status of a job.  Complexity: O(1) hash map lookup.
    JobStatus get_status(const std::string& job_id) const;

    /// List all job IDs (active + completed).  Complexity: O(n).
    std::vector<std::string> list_jobs() const;

    // ─── Observer ─────────────────────────────────────────────────────────

    /// Register a callback to receive progress updates.
    void set_progress_callback(ProgressCallback cb);

    // ─── State Persistence (future use) ───────────────────────────────────

    void save_state(const std::string& job_id) const;
    bool restore_job(const std::string& job_id);

private:
    // ─── Internal Types ───────────────────────────────────────────────────

    /// Per-job context holding segments, paths, and cancellation flag.
    struct JobContext;

    // ─── Pipeline Execution ───────────────────────────────────────────────

    /// Run the full 7-stage pipeline for one job.  Called from main_pool_.
    void run_pipeline(std::shared_ptr<JobContext> ctx);

    /// Emit progress update to registered callback.
    void emit_progress(const std::string& job_id, const std::string& stage,
                       int progress, int eta = -1);

    // ─── Thread Pools ─────────────────────────────────────────────────────
    //   main_pool_      — Runs one job at a time (sequential stages)
    //   tts_pool_       — Parallel TTS API calls  (Stage 4)
    //   translate_pool_ — Parallel translation API calls  (Stage 3)

    std::unique_ptr<ThreadPool> main_pool_;
    std::unique_ptr<ThreadPool> tts_pool_;
    std::unique_ptr<ThreadPool> translate_pool_;

    // ─── Data Stores ──────────────────────────────────────────────────────
    // DSA: unordered_map — O(1) average lookup by job_id

    mutable std::mutex jobs_mutex_;                                    ///< Guards both maps
    std::unordered_map<std::string, std::shared_ptr<JobContext>> jobs_; ///< Active jobs
    std::unordered_map<std::string, JobStatus>                  statuses_; ///< Status cache

    ProgressCallback progress_cb_;                                     ///< GUI callback

    // ─── Utility ──────────────────────────────────────────────────────────

    std::string generate_job_id() const;       ///< Random 64-bit hex string
    std::filesystem::path state_dir_;          ///< For future state persistence
};

} // namespace vd
