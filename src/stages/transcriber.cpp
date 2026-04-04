/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  transcriber.cpp — Stage 2 Implementation                                 ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * ALGORITHM:
 *
 *   1. Load GGML whisper model from disk (once per Transcriber lifetime)
 *   2. Read WAV file → skip 44-byte header → load PCM as float32
 *   3. Call whisper_full() with greedy sampling parameters
 *   4. Iterate over resulting segments:
 *      - Extract timing (centiseconds → milliseconds)
 *      - Extract text
 *      - Trim whitespace
 *      - Filter out short/empty segments
 *   5. Detect language and set on all segments
 *
 * MEMORY:
 *   - Whisper model stays loaded in impl_->ctx for the job lifetime
 *   - PCM data is loaded into float vector (WAV samples / 32768.0f)
 *
 * WHISPER TIMING:
 *   whisper_full_get_segment_t0/t1 returns time in centiseconds (10ms units)
 *   We multiply by 10 to get milliseconds for our Segment struct.
 */

#include "stages/transcriber.h"
#include "core/config_manager.h"
#include "core/logger.h"
#include "core/error_handler.h"

#include <whisper.h>
#include <vector>
#include <fstream>
#include <cstring>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// Impl — Pimpl holding whisper_context (heavy object)
// ═══════════════════════════════════════════════════════════════════════════════

struct Transcriber::Impl {
    whisper_context* ctx{nullptr};
    ~Impl() { if (ctx) whisper_free(ctx); }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor — Load whisper model from config path
// ═══════════════════════════════════════════════════════════════════════════════

Transcriber::Transcriber() : impl_(std::make_unique<Impl>()) {
    auto& cfg = ConfigManager::instance();
    std::string model_path = cfg.get_whisper_model_path();

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = cfg.get_whisper_use_gpu();

    impl_->ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (!impl_->ctx) {
        throw WhisperException(
            "Failed to load whisper model: " + model_path +
            "\n→ Download from: https://huggingface.co/ggerganov/whisper.cpp");
    }
    VD_LOG_INFO("Whisper model loaded: {}", model_path);
}

Transcriber::~Transcriber() = default;

// ═══════════════════════════════════════════════════════════════════════════════
// load_wav_as_float — Read WAV PCM into float buffer for whisper
// ═══════════════════════════════════════════════════════════════════════════════
// Whisper expects float32 samples normalized to [-1.0, 1.0].
// Conversion: float_sample = int16_sample / 32768.0f

static std::vector<float> load_wav_as_float(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) throw WhisperException("Cannot open WAV: " + path);

    f.seekg(44);  // Skip 44-byte WAV header

    // Read remaining bytes as int16_t samples
    std::vector<int16_t> pcm(std::istreambuf_iterator<char>(f), {});

    // Convert int16 → float32 normalized
    std::vector<float> out(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) {
        out[i] = static_cast<float>(pcm[i]) / 32768.0f;
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════════
// transcribe — Run whisper inference and build SegmentList
// ═══════════════════════════════════════════════════════════════════════════════

SegmentList Transcriber::transcribe(const std::string& wav_path,
                                     const std::string& language) {
    VD_LOG_INFO("Transcribing: {}", wav_path);

    // ── Load audio ───────────────────────────────────────────────────────
    auto pcm = load_wav_as_float(wav_path);

    // ── Configure whisper parameters ─────────────────────────────────────
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_realtime   = false;
    params.print_progress   = false;
    params.print_timestamps = true;
    params.translate        = false;    // We translate separately in Stage 3
    params.n_threads        = ConfigManager::instance().get_whisper_threads();
    params.token_timestamps = true;     // Need word-level timing

    if (language == "auto" || language.empty()) {
        params.language        = nullptr;  // Auto-detect language
        params.detect_language = true;
    } else {
        params.language = language.c_str();
    }

    // ── Run inference ────────────────────────────────────────────────────
    int ret = whisper_full(impl_->ctx, params,
                           pcm.data(), static_cast<int>(pcm.size()));
    if (ret != 0) {
        throw WhisperException("whisper_full failed with code " + std::to_string(ret));
    }

    // ── Extract segments ─────────────────────────────────────────────────
    int n_segs = whisper_full_n_segments(impl_->ctx);
    VD_LOG_INFO("Whisper returned {} raw segments", n_segs);

    SegmentList result;
    result.reserve(n_segs);

    for (int i = 0; i < n_segs; ++i) {
        Segment seg;
        seg.id = static_cast<uint32_t>(i);

        // Timing: centiseconds → milliseconds
        seg.start_ms = whisper_full_get_segment_t0(impl_->ctx, i) * 10;
        seg.end_ms   = whisper_full_get_segment_t1(impl_->ctx, i) * 10;

        seg.original_text = whisper_full_get_segment_text(impl_->ctx, i);
        seg.state         = Segment::State::Transcribed;

        // Trim leading/trailing whitespace
        auto& t = seg.original_text;
        t.erase(0, t.find_first_not_of(" \t\n\r"));
        t.erase(t.find_last_not_of(" \t\n\r") + 1);

        // Filter: skip empty text or ultra-short segments (noise)
        if (!t.empty() && seg.duration_ms() > 100) {
            result.push_back(std::move(seg));
        }
    }

    // ── Set detected language on all segments ────────────────────────────
    if (!result.empty()) {
        const char* detected_lang = whisper_full_lang_str(impl_->ctx, 0);
        if (detected_lang) {
            for (auto& s : result) s.source_language = detected_lang;
        }
    }

    VD_LOG_INFO("Transcription complete: {} valid segments, detected language={}",
                result.size(),
                result.empty() ? "?" : result[0].source_language);
    return result;
}

} // namespace vd
