/**
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║  rest_api_server.h — Local REST API Server for Browser Extensions         ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
 *
 * ENDPOINT TABLE:
 *
 *   Method │  Path                         │  Description
 *   ───────┼────────────────────────────── ┼──────────────────────────────
 *   POST   │  /api/v1/dub                  │  Start a new dubbing job
 *   GET    │  /api/v1/job/{id}/status       │  Get job progress/completion
 *   GET    │  /api/v1/job/{id}/download     │  Get output file path
 *   OPTIONS│  *                             │  CORS preflight response
 *
 * ARCHITECTURE:
 *   ┌─────────────────┐         ┌─────────────────────────┐
 *   │ Chrome Extension│   HTTP  │   RestApiServer          │
 *   │ (popup.js)      │────────►│   (Boost.Beast)          │
 *   └─────────────────┘         │                          │
 *                               │   Runs on localhost:7070  │
 *                               │   Accepts one conn/thread │
 *                               │                          │
 *                               │   ┌──────────────────┐   │
 *                               │   │ PipelineManager  │   │
 *                               │   │ (start_job,      │   │
 *                               │   │  get_status)     │   │
 *                               │   └──────────────────┘   │
 *                               └─────────────────────────┘
 *
 * CORS:
 *   All responses include Access-Control-Allow-Origin: *
 *   OPTIONS requests return allowed methods/headers for preflight.
 *
 * DEPENDENCIES:
 *   Boost.Beast (HTTP), Boost.Asio (async I/O), nlohmann/json, PipelineManager
 */

#pragma once

#include <memory>
#include <string>
#include "core/pipeline_manager.h"

namespace vd {

class RestApiServer {
public:
    /**
     * Create REST API server bound to PipelineManager.
     * @param pm    Reference to pipeline manager (must outlive server)
     * @param port  Port to listen on (default: 7070)
     */
    explicit RestApiServer(PipelineManager& pm, int port = 7070);
    ~RestApiServer();

    void start();                    ///< Start listening in background thread
    void stop();                     ///< Stop accepting connections and join
    bool is_running() const;         ///< Check if server is active

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vd
