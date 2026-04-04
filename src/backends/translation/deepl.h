// deepl.h
#pragma once
#include "stages/translator.h"
#include "network/http_client.h"

namespace vd {

class DeepL : public ITranslator {
public:
    DeepL();

    std::string translate(const std::string& text,
                          const std::string& src, const std::string& tgt) override;

    std::string name() const override { return "DeepL"; }

private:
    HttpClient http_;
    std::string api_key_;
    std::string api_url_;
};

} // namespace vd
