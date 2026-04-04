// libretranslate.h
#pragma once
#include "stages/translator.h"
#include "network/http_client.h"

namespace vd {

class LibreTranslate : public ITranslator {
public:
    LibreTranslate();

    std::string translate(const std::string& text,
                          const std::string& src, const std::string& tgt) override;

    std::string name() const override { return "LibreTranslate"; }

private:
    HttpClient http_;
    std::string base_url_;
};

} // namespace vd
