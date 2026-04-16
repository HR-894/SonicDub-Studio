// google_translate.cpp
#include "backends/translation/google_translate.h"
#include "core/config_manager.h"
#include "core/error_handler.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <format>
#include <curl/curl.h>

namespace vd {

GoogleTranslate::GoogleTranslate() {
    api_key_ = ConfigManager::instance().get_api_key("google_translate");
}

std::string GoogleTranslate::translate(const std::string& text,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
    if (text.empty()) return "";

    // Use unofficial free endpoint if no API key (for dev/testing)
    std::string url;
    std::string body;

    if (api_key_.empty()) {
        // Unofficial endpoint
        char* escaped_text = curl_easy_escape(nullptr, text.c_str(), static_cast<int>(text.size()));
        url = std::format(
            "https://translate.googleapis.com/translate_a/single"
            "?client=gtx&sl={}&tl={}&dt=t&q={}",
            source_lang == "auto" ? "auto" : source_lang,
            target_lang,
            escaped_text ? escaped_text : "");
        if (escaped_text) curl_free(escaped_text);

        auto resp = http_.get(url);
        if (resp.status_code != 200)
            throw NetworkException("Google Translate failed: " + std::to_string(resp.status_code));

        try {
            auto j = nlohmann::json::parse(resp.body);
            std::string result;
            for (auto& chunk : j[0]) {
                if (!chunk[0].is_null()) result += chunk[0].get<std::string>();
            }
            return result;
        } catch (const nlohmann::json::exception& e) {
            throw NetworkException(std::string("Google Translate JSON error: ") + e.what() + " Resp: " + resp.body);
        }
    }

    // Official Cloud Translation API v2
    url = "https://translation.googleapis.com/language/translate/v2?key=" + api_key_;
    nlohmann::json req_body = {
        {"q", text},
        {"target", target_lang},
        {"format", "text"}
    };
    if (source_lang != "auto") req_body["source"] = source_lang;

    auto resp = http_.post(url, req_body.dump(),
                           {{"Content-Type", "application/json"}});

    if (resp.status_code != 200)
        throw NetworkException("Google Translate API failed: " + resp.body);

    try {
        auto j = nlohmann::json::parse(resp.body);
        return j.at("data").at("translations").at(0).at("translatedText").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        throw NetworkException(std::string("Google Translate API JSON error: ") + e.what() + " Resp: " + resp.body);
    } catch (const std::exception& e) {
        throw NetworkException(std::string("Google Translate API error: ") + e.what());
    }
}

} // namespace vd
