#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

namespace vd::JsonUtils {

template<typename T>
std::optional<T> safe_get(const nlohmann::json& j, const std::string& key) {
    if (j.contains(key) && !j[key].is_null()) {
        try { return j[key].get<T>(); }
        catch (...) { return std::nullopt; }
    }
    return std::nullopt;
}

inline std::string to_string(const nlohmann::json& j) {
    return j.dump(2);
}

inline nlohmann::json parse_safe(const std::string& s) {
    try { return nlohmann::json::parse(s); }
    catch (...) { return nlohmann::json::object(); }
}

} // namespace vd::JsonUtils
