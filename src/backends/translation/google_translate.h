// google_translate.h
#pragma once
#include "stages/translator.h"
#include "network/http_client.h"

namespace vd {

class GoogleTranslate : public ITranslator {
public:
    GoogleTranslate();

    std::string translate(const std::string& text,
                          const std::string& source_lang,
                          const std::string& target_lang) override;

    std::string name() const override { return "Google Translate"; }

private:
    HttpClient http_;
    std::string api_key_;
};

} // namespace vd
