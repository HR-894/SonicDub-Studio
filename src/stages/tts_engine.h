/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  tts_engine.h — Text-to-Speech Interface (Strategy Pattern)               ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 4 of 7
 *
 *   [Translated Text] ──► ITTSEngine ──► [Audio bytes (WAV/MP3)]
 *
 * DESIGN PATTERN:  Strategy
 *   ITTSEngine is an abstract interface.  Concrete implementations:
 *     - GeminiTTS        (Google AI Studio — recommended, best quality)
 *     - GoogleCloudTTS   (Google Cloud Text-to-Speech API)
 *     - ElevenLabsTTS    (ElevenLabs multilingual voice synthesis)
 *     - EdgeTTS          (Microsoft Edge TTS — free, runs via Python)
 *
 * RETURN FORMAT:
 *   synthesize() returns raw audio bytes.  The format depends on backend:
 *     - GoogleCloudTTS: LINEAR16 PCM (direct WAV data)
 *     - GeminiTTS:      base64-decoded audio (typically WAV)
 *     - ElevenLabsTTS:  WAV stream
 *     - EdgeTTS:        MP3 → converted to WAV by the backend class
 *
 * THREAD SAFETY:
 *   Must be safe for concurrent calls from tts_pool_.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace vd {

class ITTSEngine {
public:
    virtual ~ITTSEngine() = default;

    /**
     * Synthesize text into audio.
     *
     * @param text      Text to speak
     * @param lang      ISO 639-1 language code (e.g. "hi")
     * @param voice_id  Backend-specific voice identifier
     * @return          Raw audio bytes (WAV format preferred)
     *
     * @throws TTSException on synthesis or network errors
     */
    virtual std::vector<uint8_t> synthesize(const std::string& text,
                                             const std::string& lang,
                                             const std::string& voice_id) = 0;

    /// Human-readable backend name (for logging).
    virtual std::string name() const = 0;
};

} // namespace vd
