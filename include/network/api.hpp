#ifndef VECTORFS_NETWORK_APP_HPP
#define VECTORFS_NETWORK_APP_HPP

#include "handlers.hpp"
#include <boost/regex.hpp>
#include <drogon/HttpAppFramework.h>
#include <memory>

namespace vfs::network {

template <typename EmbeddedModel> class VectorFSApi {
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

    auto addCorsOptions = [](const std::string &path) {
      drogon::app().registerHandler(
          path,
          [](const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            if (req->method() == drogon::Options) {
              auto resp = drogon::HttpResponse::newHttpResponse();
              resp->addHeader("Access-Control-Allow-Origin", "*");
              resp->addHeader("Access-Control-Allow-Methods",
                              "GET, POST, PUT, DELETE, OPTIONS");
              resp->addHeader("Access-Control-Allow-Headers",
                              "Content-Type, Authorization");
              resp->setStatusCode(drogon::k200OK);
              callback(resp);
            }
          },
          {drogon::Options});
    };

    addCorsOptions("/");
    addCorsOptions("/semantic");
    addCorsOptions("/rebuild");
    addCorsOptions("/files/.*");

    drogon::app().setCustomErrorHandler(
        [](drogon::HttpStatusCode code, const drogon::HttpRequestPtr &req) {
          Json::Value json;
          json["error"] = "Request failed";
          json["code"] = static_cast<int>(code);
          json["path"] = req->path();

          auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
          resp->addHeader("Access-Control-Allow-Origin", "*");
          resp->addHeader("Access-Control-Allow-Methods",
                          "GET, POST, PUT, DELETE, OPTIONS");
          resp->addHeader("Access-Control-Allow-Headers",
                          "Content-Type, Authorization");
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

    drogon::app().registerHandler(
        "/files/create", handler::handlers.at("/files/create"), {drogon::Post});
    drogon::app().registerHandler(
        "/files/read", handler::handlers.at("/files/read"), {drogon::Get});

    spdlog::info("VectorFS API initialized with CORS support");
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