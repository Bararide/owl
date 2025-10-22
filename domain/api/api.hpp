#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "infrastructure/result.hpp"
#include "instance/instance.hpp"
#include "shared_memory/shared_memory.hpp"
#include "utils/http_helpers.hpp"

#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/net.h>
#include <pistache/router.h>

#include <atomic>
#include <memory>
#include <thread>

namespace owl::network {

template <typename EmbeddedModel> class VectorFSApi {
public:
  VectorFSApi()
      : httpEndpoint(std::make_unique<Pistache::Http::Endpoint>(
            Pistache::Address("0.0.0.0", 9999))) {}

  void init() {
    spdlog::info("Initializing Pistache API...");

    auto opts = Pistache::Http::Endpoint::options()
                    .threads(std::thread::hardware_concurrency())
                    .flags(Pistache::Tcp::Options::ReuseAddr);

    httpEndpoint->init(opts);
    setupRoutes();

    spdlog::info("Pistache API initialized successfully");
  }

  void run() {
    spdlog::info("Starting Pistache server on port 9999");
    httpEndpoint->setHandler(router.handler());
    httpEndpoint->serve();
  }

  void shutdown() {
    spdlog::info("Shutting down Pistache server");
    httpEndpoint->shutdown();
  }

private:
  void setupRoutes() {
    using namespace Pistache::Rest;

    Routes::Options(router, "/*", Routes::bind(&VectorFSApi::handleCors, this));

    Routes::Get(router, "/", Routes::bind(&VectorFSApi::handleRoot, this));
    Routes::Post(router, "/files/create",
                 Routes::bind(&VectorFSApi::handleFileCreate, this));
    // Routes::Post(router, "/create/container",
    //              Router::bind(&VectorFSApi::handleContainerCreate, this));
    Routes::Get(router, "/files/read",
                Routes::bind(&VectorFSApi::handleFileRead, this));
    Routes::Post(router, "/semantic",
                 Routes::bind(&VectorFSApi::handleSemanticSearch, this));
    Routes::Post(router, "/rebuild",
                 Routes::bind(&VectorFSApi::handleRebuild, this));

    spdlog::info("Routes registered");
  }

  void addCorsHeaders(Pistache::Http::ResponseWriter &response) {
    response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>(
        "*");
    response.headers().add<Pistache::Http::Header::AccessControlAllowMethods>(
        "GET, POST, PUT, DELETE, OPTIONS");
    response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>(
        "Content-Type, Authorization");
  }

  void handleCors(const Pistache::Rest::Request &request,
                  Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);
    response.send(Pistache::Http::Code::Ok);
  }

  void handleRoot(const Pistache::Rest::Request &request,
                  Pistache::Http::ResponseWriter response) {
    spdlog::info("=== ROOT HANDLER CALLED ===");
    spdlog::info("Path: {}", request.resource());
    spdlog::info("Client: {}", request.address().host());

    addCorsHeaders(response);
    response.send(Pistache::Http::Code::Ok, "OK");

    spdlog::info("=== ROOT HANDLER COMPLETED ===");
  }

  core::Result<Json::Value, std::string>
  parseJsonBody(const std::string &body) {
    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(body, json)) {
      return core::Result<Json::Value, std::string>::Error("Invalid JSON");
    }
    return core::Result<Json::Value, std::string>::Ok(std::move(json));
  }

  core::Result<std::pair<std::string, std::string>, std::string>
  validateFileCreateParams(const Json::Value &json) {
    if (!json.isMember("path") || !json.isMember("content")) {
      return core::Result<std::pair<std::string, std::string>,
                          std::string>::Error("Missing 'path' or 'content'");
    }

    std::string path = json["path"].asString();
    std::string content = json["content"].asString();

    if (path.empty()) {
      return core::Result<std::pair<std::string, std::string>,
                          std::string>::Error("Path cannot be empty");
    }

    if (!path.empty() && path[0] != '/') {
      path = "/" + path;
    }

    return core::Result<std::pair<std::string, std::string>, std::string>::Ok(
        std::make_pair(std::move(path), std::move(content)));
  }

  core::Result<bool, std::string> initializeSharedMemory() {
    auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
    if (!shm_manager.initialize()) {
      return core::Result<bool, std::string>::Error(
          "Failed to initialize shared memory");
    }
    return core::Result<bool, std::string>::Ok(true);
  }

  core::Result<bool, std::string>
  addFileToSharedMemory(const std::string &path, const std::string &content) {
    auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
    if (!shm_manager.addFile(path, content)) {
      return core::Result<bool, std::string>::Error(
          "Failed to add file to shared memory");
    }
    return core::Result<bool, std::string>::Ok(true);
  }

  void handleFileCreate(const Pistache::Rest::Request &request,
                        Pistache::Http::ResponseWriter response) {
    spdlog::info("=== File Creation Request ===");
    spdlog::info("Client IP: {}", request.address().host());
    spdlog::info("URL: {}", request.resource());

    addCorsHeaders(response);

    auto result =
        parseJsonBody(request.body())
            .and_then([this](Json::Value json)
                          -> core::Result<std::pair<std::string, std::string>,
                                          std::string> {
              return validateFileCreateParams(json);
            })
            .and_then([this](std::pair<std::string, std::string> params)
                          -> core::Result<bool, std::string> {
              auto [path, content] = params;

              spdlog::info("File path: {}", path);
              spdlog::info("Content length: {} bytes", content.size());

              return initializeSharedMemory().and_then(
                  [this, path = std::move(path),
                   content = std::move(
                       content)]() mutable -> core::Result<bool, std::string> {
                    return addFileToSharedMemory(path, content);
                  });
            })
            .map([&request](bool success) -> Json::Value {
              spdlog::info("Successfully added file to shared memory");
              return utils::create_success_response(
                  {"path", "size", "created"}, "path_placeholder",
                  static_cast<Json::UInt64>(0), true);
            });

    result.handle(
        [&response](Json::Value &data) {
          response.send(Pistache::Http::Code::Ok, data.toStyledString());
        },
        [&response](std::string &error) {
          spdlog::warn("File creation failed: {}", error);
          auto error_response = utils::create_error_response(error);
          response.send(Pistache::Http::Code::Bad_Request,
                        error_response.toStyledString());
        });
  }

  core::Result<std::string, std::string>
  getPathFromQuery(const Pistache::Rest::Request &request) {
    auto path_param = request.query().get("path");
    if (!path_param || path_param->empty()) {
      return core::Result<std::string, std::string>::Error(
          "Path parameter is required");
    }
    return core::Result<std::string, std::string>::Ok(*path_param);
  }

  core::Result<std::string, std::string>
  getFileContent(const std::string &path) {
    try {
      auto &search =
          owl::instance::VFSInstance<EmbeddedModel>::getInstance().get_search();
      const std::string &content = search.getFileContentImpl(path);
      return core::Result<std::string, std::string>::Ok(content);
    } catch (const std::exception &e) {
      return core::Result<std::string, std::string>::Error("File not found");
    }
  }

  void handleFileRead(const Pistache::Rest::Request &request,
                      Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);

    auto path_result = getPathFromQuery(request);
    if (!path_result.is_ok()) {
      auto error_response = utils::create_error_response(path_result.error());
      response.send(Pistache::Http::Code::Bad_Request,
                    error_response.toStyledString());
      return;
    }

    auto content_result = getFileContent(path_result.value());
    if (!content_result.is_ok()) {
      auto error_response =
          utils::create_error_response(content_result.error());
      response.send(Pistache::Http::Code::Not_Found,
                    error_response.toStyledString());
      return;
    }

    auto result =
        content_result.map([&request](std::string content) -> Json::Value {
          auto path_param = request.query().get("path").value_or("");
          return utils::create_success_response(
              {"path", "content", "size"}, path_param, content,
              static_cast<Json::UInt64>(content.size()));
        });

    result.handle(
        [&response](Json::Value &data) {
          response.send(Pistache::Http::Code::Ok, data.toStyledString());
        },
        [&response](std::string &error) {
          spdlog::error("File read error: {}", error);
          auto error_response = utils::create_error_response(error);
          response.send(Pistache::Http::Code::Not_Found,
                        error_response.toStyledString());
        });
  }

  core::Result<std::pair<std::string, int>, std::string>
  validateSemanticSearchParams(const Json::Value &json) {
    if (!json.isMember("query")) {
      return core::Result<std::pair<std::string, int>, std::string>::Error(
          "Missing 'query'");
    }

    const std::string query = json["query"].asString();
    int limit = json.get("limit", 5).asInt();

    if (query.empty()) {
      return core::Result<std::pair<std::string, int>, std::string>::Error(
          "Query cannot be empty");
    }

    return core::Result<std::pair<std::string, int>, std::string>::Ok(
        std::make_pair(query, limit));
  }

  void handleSemanticSearch(const Pistache::Rest::Request &request,
                            Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);

    auto result =
        parseJsonBody(request.body())
            .and_then(
                [this](Json::Value json)
                    -> core::Result<std::pair<std::string, int>, std::string> {
                  return validateSemanticSearchParams(json);
                })
            .map([this](std::pair<std::string, int> params) -> Json::Value {
              auto [query, limit] = params;

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
              auto results = vfs.get_search().semanticSearchImpl(query, limit);

              Json::Value resultsJson(Json::arrayValue);
              for (const auto &[path, score] : results) {
                Json::Value resultJson;
                resultJson["path"] = path;
                resultJson["score"] = score;
                resultsJson.append(resultJson);
              }

              return utils::create_success_response(
                  {"query", "results", "count"}, query, resultsJson,
                  static_cast<int>(results.size()));
            });

    result.handle(
        [&response](Json::Value &data) {
          response.send(Pistache::Http::Code::Ok, data.toStyledString());
        },
        [&response](std::string &error) {
          spdlog::error("Semantic search error: {}", error);
          auto error_response = utils::create_error_response(error);
          response.send(Pistache::Http::Code::Bad_Request,
                        error_response.toStyledString());
        });
  }

  void handleRebuild(const Pistache::Rest::Request &request,
                     Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);

    auto response_data =
        utils::create_success_response({"message"}, "Rebuild completed");
    response.send(Pistache::Http::Code::Ok, response_data.toStyledString());
  }

private:
  std::unique_ptr<Pistache::Http::Endpoint> httpEndpoint;
  Pistache::Rest::Router router;
};

} // namespace owl::network

#endif // VECTORFS_PISTACHE_API_HPP