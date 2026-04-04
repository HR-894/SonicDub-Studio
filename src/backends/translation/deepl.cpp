// deepl.cpp
#include "backends/translation/deepl.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include <nlohmann/json.hpp>

namespace vd {

DeepL::DeepL() {
    api_key_ = ConfigManager::instance().get_api_key("deepl");
    // Free tier uses api-free.deepl.com
    bool is_free = api_key_.ends_with(":fx");
    api_url_ = is_free ? "https://api-free.deepl.com" : "https://api.deepl.com";
}

std::string DeepL::translate(const std::string& text,
                             const std::string& src, const std::string& tgt) {
    if (text.empty()) return "";

    nlohmann::json body = {
        {"text", nlohmann::json::array({text})},
        {"target_lang", tgt == "hi" ? "HI" : tgt}
    };
    if (src != "auto") body["source_lang"] = src;

    auto resp = http_.post(api_url_ + "/v2/translate", body.dump(), {
        {"Authorization", "DeepL-Auth-Key " + api_key_},
        {"Content-Type", "application/json"}
    });

    if (resp.status_code != 200)
        throw NetworkException("DeepL failed: " + resp.body);

    auto j = nlohmann::json::parse(resp.body);
    return j["translations"][0]["text"].get<std::string>();
}

} // namespace vd
