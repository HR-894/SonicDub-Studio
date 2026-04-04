// rest_api_server.cpp
#include "network/rest_api_server.h"
#include "core/logger.h"
#include "core/config_manager.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <thread>
#include <atomic>
#include <fstream>
#include <filesystem>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;
using json      = nlohmann::json;

namespace vd {

struct RestApiServer::Impl {
    PipelineManager& pm;
    int port;
    std::atomic<bool> running{false};
    std::thread server_thread;
    net::io_context ioc;

    explicit Impl(PipelineManager& p, int port_) : pm(p), port(port_) {}

    void handle_session(tcp::socket socket) {
        try {
            beast::flat_buffer buf;
            http::request<http::string_body> req;
            http::read(socket, buf, req);

            http::response<http::string_body> res;
            res.version(req.version());
            res.set(http::field::server, "VideoDubber/1.0");
            res.set(http::field::content_type, "application/json");
            res.set(http::field::access_control_allow_origin, "*");
            res.set(http::field::access_control_allow_methods, "GET,POST,OPTIONS");
            res.set(http::field::access_control_allow_headers, "Content-Type");

            std::string target = std::string(req.target());

            if (req.method() == http::verb::options) {
                res.result(http::status::ok);
                res.body() = "";
            }
            else if (req.method() == http::verb::post && target == "/api/v1/dub") {
                try {
                    auto body = json::parse(req.body());
                    JobConfig cfg;
                    cfg.input_path           = body.value("url", "");
                    cfg.source_language      = body.value("source_lang", "auto");
                    cfg.target_language      = body.value("target_lang", "hi");
                    cfg.tts_backend          = body.value("tts_backend",
                        ConfigManager::instance().get_tts_backend());
                    cfg.translation_backend  = body.value("translation_backend",
                        ConfigManager::instance().get_translation_backend());
                    cfg.audio_mix_mode       = body.value("mix_mode", "overlay");

                    auto stem = std::filesystem::path(cfg.input_path).stem().string();
                    cfg.output_path = ConfigManager::instance().get_output_dir()
                        + "/" + stem + "_dubbed.mp4";

                    std::string job_id = pm.start_job(cfg);
                    res.result(http::status::ok);
                    res.body() = json{{"job_id", job_id}}.dump();
                } catch (const std::exception& e) {
                    res.result(http::status::bad_request);
                    res.body() = json{{"error", e.what()}}.dump();
                }
            }
            else if (req.method() == http::verb::get
                     && target.starts_with("/api/v1/job/")) {
                // Parse job_id and sub-resource
                auto rest = target.substr(12); // after /api/v1/job/
                auto slash = rest.find('/');
                std::string job_id   = slash == std::string::npos ? rest : rest.substr(0, slash);
                std::string resource = slash == std::string::npos ? "status" : rest.substr(slash + 1);

                auto status = pm.get_status(job_id);

                if (resource == "status") {
                    res.result(http::status::ok);
                    res.body() = json{
                        {"job_id",      status.job_id},
                        {"stage",       status.stage},
                        {"progress",    status.progress},
                        {"eta_seconds", status.eta_seconds},
                        {"completed",   status.completed},
                        {"failed",      status.failed},
                        {"error",       status.error_message}
                    }.dump();
                } else if (resource == "download") {
                    if (!status.completed) {
                        res.result(http::status::accepted);
                        res.body() = json{{"error","not ready"}}.dump();
                    } else {
                        res.result(http::status::ok);
                        res.body() = json{{"path", status.output_path}}.dump();
                    }
                } else {
                    res.result(http::status::not_found);
                    res.body() = json{{"error","unknown resource"}}.dump();
                }
            }
            else {
                res.result(http::status::not_found);
                res.body() = json{{"error","not found"}}.dump();
            }

            res.content_length(res.body().size());
            http::write(socket, res);
        } catch (const std::exception& e) {
            VD_LOG_WARN("REST session error: {}", e.what());
        }
    }

    void run() {
        try {
            tcp::acceptor acceptor(ioc, {tcp::v4(), static_cast<uint16_t>(port)});
            VD_LOG_INFO("REST API listening on http://localhost:{}", port);

            while (running) {
                tcp::socket socket(ioc);
                acceptor.accept(socket);
                std::thread([this, s = std::move(socket)]() mutable {
                    handle_session(std::move(s));
                }).detach();
            }
        } catch (const std::exception& e) {
            VD_LOG_ERROR("REST API error: {}", e.what());
        }
    }
};

RestApiServer::RestApiServer(PipelineManager& pm, int port)
    : impl_(std::make_unique<Impl>(pm, port)) {}

RestApiServer::~RestApiServer() { stop(); }

void RestApiServer::start() {
    impl_->running = true;
    impl_->server_thread = std::thread([this]{ impl_->run(); });
}

void RestApiServer::stop() {
    impl_->running = false;
    impl_->ioc.stop();
    if (impl_->server_thread.joinable()) impl_->server_thread.join();
}

bool RestApiServer::is_running() const { return impl_->running; }

} // namespace vd
