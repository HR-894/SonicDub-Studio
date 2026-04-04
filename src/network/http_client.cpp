// http_client.cpp
#include "network/http_client.h"
#include "core/error_handler.h"
#include "core/logger.h"
#include <curl/curl.h>
#include <sstream>

namespace vd {

struct HttpClient::Impl {
    CURL* curl{nullptr};
    long timeout_sec{30};
    std::string user_agent{"VideoDubber/1.0"};

    Impl() {
        curl = curl_easy_init();
        if (!curl) throw NetworkException("Failed to init libcurl");
    }
    ~Impl() { if (curl) curl_easy_cleanup(curl); }
};

static size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

static size_t header_callback(char* buf, size_t size, size_t nitems,
                               std::map<std::string, std::string>* headers) {
    std::string line(buf, size * nitems);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 2);
        value.erase(value.find_last_not_of(" \r\n") + 1);
        (*headers)[key] = value;
    }
    return size * nitems;
}

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {}
HttpClient::~HttpClient() = default;

void HttpClient::set_timeout(long t) { impl_->timeout_sec = t; }
void HttpClient::set_user_agent(const std::string& ua) { impl_->user_agent = ua; }

HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& req_headers) {
    CURL* curl = impl_->curl;
    HttpResponse resp;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp.headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, impl_->timeout_sec);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, impl_->user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* hlist = nullptr;
    for (auto& [k, v] : req_headers) {
        hlist = curl_slist_append(hlist, (k + ": " + v).c_str());
    }
    if (hlist) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    CURLcode res = curl_easy_perform(curl);
    if (hlist) curl_slist_free_all(hlist);

    if (res != CURLE_OK) throw NetworkException(curl_easy_strerror(res));
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);

    return resp;
}

HttpResponse HttpClient::post(const std::string& url,
                              const std::string& body,
                              const std::map<std::string, std::string>& req_headers) {
    CURL* curl = impl_->curl;
    HttpResponse resp;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, impl_->timeout_sec);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, impl_->user_agent.c_str());

    struct curl_slist* hlist = nullptr;
    for (auto& [k, v] : req_headers) {
        hlist = curl_slist_append(hlist, (k + ": " + v).c_str());
    }
    if (hlist) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    CURLcode res = curl_easy_perform(curl);
    if (hlist) curl_slist_free_all(hlist);

    if (res != CURLE_OK) throw NetworkException(curl_easy_strerror(res));
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);

    return resp;
}

HttpResponse HttpClient::post_binary(const std::string& url,
                                     const std::vector<uint8_t>& body,
                                     const std::map<std::string, std::string>& req_headers) {
    CURL* curl = impl_->curl;
    HttpResponse resp;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, impl_->timeout_sec);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, impl_->user_agent.c_str());

    struct curl_slist* hlist = nullptr;
    for (auto& [k, v] : req_headers) {
        hlist = curl_slist_append(hlist, (k + ": " + v).c_str());
    }
    if (hlist) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    CURLcode res = curl_easy_perform(curl);
    if (hlist) curl_slist_free_all(hlist);

    if (res != CURLE_OK) throw NetworkException(curl_easy_strerror(res));
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);

    return resp;
}

} // namespace vd
