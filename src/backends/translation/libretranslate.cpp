// libretranslate.cpp
#include "backends/translation/libretranslate.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include <nlohmann/json.hpp>

namespace vd {

LibreTranslate::LibreTranslate() {
    base_url_ = ConfigManager::instance().get_libretranslate_url();
}

std::string LibreTranslate::translate(const std::string& text,
                                      const std::string& src,
                                      const std::string& tgt) {
    if (text.empty()) return "";

    nlohmann::json body = {
        {"q", text},
        {"source", src == "auto" ? "en" : src},
        {"target", tgt},
        {"format", "text"}
    };

    auto resp = http_.post(base_url_ + "/translate", body.dump(),
                           {{"Content-Type","application/json"}});

    if (resp.status_code != 200)
        throw NetworkException("LibreTranslate failed: " + resp.body);

    try {
        auto j = nlohmann::json::parse(resp.body);
        return j.at("translatedText").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw NetworkException(std::string("LibreTranslate API JSON error: ") + e.what() + " Resp: " + resp.body);
    } catch (const std::exception& e) {
        throw NetworkException(std::string("LibreTranslate API error: ") + e.what());
    }
}

} // namespace vd
