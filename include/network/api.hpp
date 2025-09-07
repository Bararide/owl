#ifndef VECTORFS_NETWORK_APP_HPP
#define VECTORFS_NETWORK_APP_HPP

#include "handlers.hpp"
#include <boost/regex.hpp>
#include <drogon/HttpAppFramework.h>
#include <memory>

namespace vfs::network {

class VectorFSApi {
public:
  static void init() {
    using namespace std::chrono_literals;

    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr &,
           const drogon::HttpResponsePtr &resp) {
          resp->addHeader("Access-Control-Allow-Origin", "*");
          resp->addHeader("Access-Control-Allow-Methods",
                          "GET, POST, PUT, DELETE, OPTIONS");
          resp->addHeader("Access-Control-Allow-Headers",
                          "Content-Type, Authorization");
        });

    drogon::app().setCustomErrorHandler(
        [](drogon::HttpStatusCode code, const drogon::HttpRequestPtr &req) {
          Json::Value json;
          json["error"] = "Request failed";
          json["code"] = static_cast<int>(code);
          json["path"] = req->path();

          auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(code);
          return resp;
        });

    drogon::app().setLogLevel(trantor::Logger::kInfo);

    drogon::app().registerHandler("/", handler::handlers.at("/"),
                                  {drogon::Get});
    drogon::app().registerHandler(
        "/semantic", handler::handlers.at("/semantic"), {drogon::Post});
    drogon::app().registerHandler("/rebuild", handler::handlers.at("/rebuild"),
                                  {drogon::Post});

    auto file_handler = handler::handlers.at("/files/.*");
    drogon::app().registerHandler("/files/.*", file_handler, {drogon::Get});
    drogon::app().registerHandler("/files/.*", file_handler, {drogon::Post});

    spdlog::info("VectorFS API initialized with {} handlers",
                 handler::handlers.size());
  }

  static void run() {
    using namespace std::chrono_literals;

    drogon::app()
        .setDocumentRoot("./www")
        .setClientMaxBodySize(20 * 1024 * 1024)
        .setClientMaxMemoryBodySize(4 * 1024 * 1024)
        .addListener("0.0.0.0", 9999)
        .setThreadNum(std::thread::hardware_concurrency())
        .setIdleConnectionTimeout(60s)
        .run();
  }
};

} // namespace vfs::network

#endif