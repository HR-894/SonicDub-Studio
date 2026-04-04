/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  translator.h — Translation Interface (Strategy Pattern)                  ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * PIPELINE POSITION:  Stage 3 of 7
 *
 *   [Transcribed Text] ──► ITranslator ──► [Translated Text]
 *
 * DESIGN PATTERN:  Strategy
 *   ITranslator is an abstract interface.  Concrete implementations:
 *     - GoogleTranslate      (official API or free endpoint)
 *     - DeepL                (pro/free API)
 *     - LibreTranslate       (self-hosted open source)
 *
 *   The PipelineManager's make_translator() factory selects the backend
 *   based on config, so pipeline code never knows which backend is active.
 *
 * THREAD SAFETY:
 *   Implementations must be safe for concurrent calls from translate_pool_.
 *   Each call creates its own HTTP request (no shared mutable state).
 */

#pragma once

#include <string>

namespace vd {

class ITranslator {
public:
    virtual ~ITranslator() = default;

    /**
     * Translate text from one language to another.
     *
     * @param text         Source text to translate
     * @param source_lang  ISO 639-1 code (e.g. "en") or "auto"
     * @param target_lang  ISO 639-1 code (e.g. "hi")
     * @return             Translated text
     *
     * @throws NetworkException on HTTP errors
     */
    virtual std::string translate(const std::string& text,
                                  const std::string& source_lang,
                                  const std::string& target_lang) = 0;

    /// Human-readable backend name (for logging).
    virtual std::string name() const = 0;
};

} // namespace vd
