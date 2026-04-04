/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  audio_extractor.cpp — Stage 1 Implementation                             ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * DETAILED ALGORITHM:
 *
 *   ┌──────────────────────────────────────────────────────┐
 *   │  1. avformat_open_input()    — Open container        │
 *   │  2. av_find_best_stream()    — Find audio stream     │
 *   │  3. avcodec_alloc_context3() — Alloc decoder context │
 *   │  4. swr_alloc_set_opts2()    — Setup resampler       │
 *   │                                                      │
 *   │  DECODE LOOP:                                        │
 *   │  ┌──────────────────────────────────────────┐        │
 *   │  │  while av_read_frame() == 0:             │        │
 *   │  │    avcodec_send_packet()                 │        │
 *   │  │    while avcodec_receive_frame() == 0:   │        │
 *   │  │      swr_convert(frame → 16kHz mono)     │        │
 *   │  │      append to pcm_data vector           │        │
 *   │  └──────────────────────────────────────────┘        │
 *   │                                                      │
 *   │  5. Flush SWR (get remaining buffered samples)       │
 *   │  6. Write 44-byte WAV header                         │
 *   │  7. Write pcm_data as raw bytes                      │
 *   └──────────────────────────────────────────────────────┘
 *
 * WAV FILE FORMAT (RIFF):
 *   Offset  Size  Field
 *   ──────  ────  ─────────────────
 *   0       4     "RIFF"
 *   4       4     file_size - 8
 *   8       4     "WAVE"
 *   12      4     "fmt "
 *   16      4     16 (subchunk size)
 *   20      2     1  (PCM format)
 *   22      2     1  (mono)
 *   24      4     16000 (sample rate)
 *   28      4     32000 (byte rate)
 *   32      2     2  (block align)
 *   34      2     16 (bits per sample)
 *   36      4     "data"
 *   40      4     data_size
 *   44      ...   raw PCM samples
 *
 * MEMORY:
 *   Pre-allocates pcm_data for ~5 minutes of audio (16000 × 300 samples).
 *   For longer videos, vector grows with amortized O(1) reallocation.
 */

#include "stages/audio_extractor.h"
#include "utils/ffmpeg_wrapper.h"
#include "core/logger.h"
#include "core/error_handler.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <fstream>
#include <vector>

namespace vd {

void AudioExtractor::extract(const std::string& input_path,
                              const std::string& output_wav) {
    VD_LOG_INFO("Extracting audio: {} → {}", input_path, output_wav);
    extract_via_ffmpeg(input_path, output_wav);
}

void AudioExtractor::extract_via_ffmpeg(const std::string& input,
                                         const std::string& output) {

    // ── Step 1: Open input file ──────────────────────────────────────────
    auto fmt_ctx  = open_input(input);
    int audio_idx = find_audio_stream(fmt_ctx.get());
    auto dec_ctx  = open_decoder(fmt_ctx.get(), audio_idx);

    // ── Step 2: Configure resampler ──────────────────────────────────────
    // Target: s16le, 16kHz, mono (optimal for whisper.cpp)
    const int            OUT_SAMPLE_RATE = 16000;
    const int            OUT_CHANNELS    = 1;
    const AVSampleFormat OUT_FMT         = AV_SAMPLE_FMT_S16;

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    SwrContext* swr_raw = nullptr;
    swr_alloc_set_opts2(&swr_raw,
        &out_ch_layout,    OUT_FMT, OUT_SAMPLE_RATE,   // destination
        &dec_ctx->ch_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate, // source
        0, nullptr);
    SwrContextPtr swr(swr_raw);
    int ret = swr_init(swr.get());
    VD_CHECK_FFMPEG(ret, "swr_init failed");

    // ── Step 3: Decode + Resample loop ───────────────────────────────────
    // DSA: vector<int16_t> — contiguous memory, O(1) amortized push_back
    std::vector<int16_t> pcm_data;
    pcm_data.reserve(OUT_SAMPLE_RATE * 300);  // Pre-alloc ~5 minutes

    auto pkt   = AVPacketPtr(av_packet_alloc());
    auto frame = AVFramePtr(av_frame_alloc());

    while (av_read_frame(fmt_ctx.get(), pkt.get()) >= 0) {
        // Skip non-audio packets
        if (pkt->stream_index != audio_idx) {
            av_packet_unref(pkt.get());
            continue;
        }

        ret = avcodec_send_packet(dec_ctx.get(), pkt.get());
        av_packet_unref(pkt.get());
        if (ret < 0) continue;

        // Drain all available frames from decoder
        while ((ret = avcodec_receive_frame(dec_ctx.get(), frame.get())) == 0) {
            // Calculate output sample count after resampling
            int out_samples = av_rescale_rnd(
                swr_get_delay(swr.get(), dec_ctx->sample_rate) + frame->nb_samples,
                OUT_SAMPLE_RATE, dec_ctx->sample_rate, AV_ROUND_UP);

            std::vector<int16_t> buf(out_samples * OUT_CHANNELS);
            uint8_t* out_buf = reinterpret_cast<uint8_t*>(buf.data());

            int converted = swr_convert(swr.get(), &out_buf, out_samples,
                                        const_cast<const uint8_t**>(frame->data),
                                        frame->nb_samples);

            if (converted > 0) {
                pcm_data.insert(pcm_data.end(), buf.begin(), buf.begin() + converted);
            }
            av_frame_unref(frame.get());
        }
    }

    // ── Step 3b: FLUSH DECODER (critical — prevents audio truncation) ────
    // After av_read_frame() returns EOF, the decoder still holds buffered
    // frames internally. Without this flush, the last 50-200ms of audio
    // is silently discarded, causing an audible cut-off at the end.
    avcodec_send_packet(dec_ctx.get(), nullptr);  // Signal EOF to decoder
    while (avcodec_receive_frame(dec_ctx.get(), frame.get()) == 0) {
        int out_samples = av_rescale_rnd(
            swr_get_delay(swr.get(), dec_ctx->sample_rate) + frame->nb_samples,
            OUT_SAMPLE_RATE, dec_ctx->sample_rate, AV_ROUND_UP);

        std::vector<int16_t> buf(out_samples * OUT_CHANNELS);
        uint8_t* out_buf = reinterpret_cast<uint8_t*>(buf.data());

        int converted = swr_convert(swr.get(), &out_buf, out_samples,
                                    const_cast<const uint8_t**>(frame->data),
                                    frame->nb_samples);
        if (converted > 0) {
            pcm_data.insert(pcm_data.end(), buf.begin(), buf.begin() + converted);
        }
        av_frame_unref(frame.get());
    }

    // ── Step 4: Flush SWR (get remaining buffered samples) ───────────────
    while (true) {
        std::vector<int16_t> buf(1024);
        uint8_t* out_buf = reinterpret_cast<uint8_t*>(buf.data());
        int n = swr_convert(swr.get(), &out_buf, 1024, nullptr, 0);
        if (n <= 0) break;
        pcm_data.insert(pcm_data.end(), buf.begin(), buf.begin() + n);
    }

    // ── Step 5: Write WAV file ───────────────────────────────────────────
    std::ofstream wav(output, std::ios::binary);
    uint32_t data_size      = static_cast<uint32_t>(pcm_data.size() * sizeof(int16_t));
    uint32_t file_size      = 36 + data_size;
    uint32_t byte_rate      = OUT_SAMPLE_RATE * OUT_CHANNELS * sizeof(int16_t);
    uint16_t block_align    = OUT_CHANNELS * sizeof(int16_t);
    uint16_t bits_per_sample = 16;
    uint16_t audio_format   = 1;  // PCM

    auto write32 = [&](uint32_t v) { wav.write(reinterpret_cast<const char*>(&v), 4); };
    auto write16 = [&](uint16_t v) { wav.write(reinterpret_cast<const char*>(&v), 2); };

    // RIFF header
    wav.write("RIFF", 4);  write32(file_size);
    wav.write("WAVE", 4);
    wav.write("fmt ", 4);  write32(16);
    write16(audio_format);  write16(static_cast<uint16_t>(OUT_CHANNELS));
    write32(OUT_SAMPLE_RATE);  write32(byte_rate);
    write16(block_align);  write16(bits_per_sample);
    wav.write("data", 4);  write32(data_size);

    // Raw PCM data
    wav.write(reinterpret_cast<const char*>(pcm_data.data()), data_size);

    VD_LOG_INFO("Audio extracted: {} samples at {}Hz ({:.1f} seconds)",
                pcm_data.size(), OUT_SAMPLE_RATE,
                static_cast<double>(pcm_data.size()) / OUT_SAMPLE_RATE);
}

} // namespace vd
