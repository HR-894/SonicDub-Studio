/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  http_client.h — Thread-Safe HTTP Client (libcurl Wrapper)                ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * FILE PURPOSE:
 *   Wraps libcurl in a clean C++ interface for making HTTP GET/POST requests.
 *   Used by all translation and TTS backends to call cloud APIs.
 *
 * DESIGN:
 *   ┌──────────────┐         ┌────────────────┐
 *   │  TTS Backend │────────►│   HttpClient   │────► HTTPS ────► API Server
 *   │  (any)       │         │  (libcurl)     │                   (Google, etc.)
 *   └──────────────┘         └────────────────┘
 *
 * PIMPL PATTERN:
 *   Implementation details (CURL handle, callbacks) are hidden behind Impl.
 *   Callers only see clean C++ types (HttpResponse, string maps).
 *
 * THREAD SAFETY:
 *   Each HttpClient instance has its own CURL handle.
 *   Each backend creates its own HttpClient, so no cross-thread sharing.
 *   However, libcurl itself is globally initialized once (by CURL::libcurl cmake).
 *
 * DEPENDENCIES:
 *   libcurl (linked via CURL::libcurl)
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <memory>

namespace vd {

// ═══════════════════════════════════════════════════════════════════════════════
// HttpResponse — Return type for all HTTP calls
// ═══════════════════════════════════════════════════════════════════════════════

struct HttpResponse {
    int                               status_code{0};   ///< HTTP status (200, 401, etc.)
    std::string                       body;             ///< Response body (JSON, binary, etc.)
    std::map<std::string, std::string> headers;          ///< Response headers
};

// ═══════════════════════════════════════════════════════════════════════════════
// HttpClient — Clean C++ wrapper around libcurl
// ═══════════════════════════════════════════════════════════════════════════════

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Non-copyable (owns CURL handle)
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // ─── Request Methods ──────────────────────────────────────────────────

    /// HTTP GET request.
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers = {});

    /// HTTP POST with string body (typically JSON).
    HttpResponse post(const std::string& url,
                      const std::string& body,
                      const std::map<std::string, std::string>& headers = {});

    /// HTTP POST with binary body (e.g. audio upload).
    HttpResponse post_binary(const std::string& url,
                             const std::vector<uint8_t>& body,
                             const std::map<std::string, std::string>& headers = {});

    // ─── Configuration ────────────────────────────────────────────────────

    void set_timeout(long timeout_sec);        ///< Request timeout (default: 30s)
    void set_user_agent(const std::string& ua); ///< User-Agent header

private:
    struct Impl;                    ///< Forward-declared (hides curl.h)
    std::unique_ptr<Impl> impl_;
};

} // namespace vd
