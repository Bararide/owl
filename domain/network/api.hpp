#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

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

  void handleFileCreate(const Pistache::Rest::Request &request,
                        Pistache::Http::ResponseWriter response) {
    spdlog::info("=== File Creation Request ===");
    spdlog::info("Client IP: {}", request.address().host());
    spdlog::info("URL: {}", request.resource());

    addCorsHeaders(response);

    try {
      auto body = request.body();
      spdlog::info("Received body: {}", body);

      Json::Value json;
      Json::Reader reader;
      if (!reader.parse(body, json)) {
        spdlog::warn("Invalid JSON received");
        auto error_response = utils::create_error_response("Invalid JSON");
        response.send(Pistache::Http::Code::Bad_Request,
                      error_response.toStyledString());
        return;
      }

      if (!json.isMember("path") || !json.isMember("content")) {
        spdlog::warn("Missing required fields");
        auto error_response =
            utils::create_error_response("Missing 'path' or 'content'");
        response.send(Pistache::Http::Code::Bad_Request,
                      error_response.toStyledString());
        return;
      }

      std::string path = json["path"].asString();
      std::string content = json["content"].asString();

      if (!path.empty() && path[0] != '/') {
        path = "/" + path;
        spdlog::info("Normalized path to: {}", path);
      }

      spdlog::info("File path: {}", path);
      spdlog::info("Content length: {} bytes", content.size());

      auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      if (!shm_manager.initialize()) {
        spdlog::error("Failed to initialize shared memory");
        auto error_response =
            utils::create_error_response("Internal server error");
        response.send(Pistache::Http::Code::Internal_Server_Error,
                      error_response.toStyledString());
        return;
      }

      if (!shm_manager.addFile(path, content)) {
        spdlog::error("Failed to add file to shared memory: {}", path);
        auto error_response =
            utils::create_error_response("Failed to create file");
        response.send(Pistache::Http::Code::Internal_Server_Error,
                      error_response.toStyledString());
        return;
      }

      spdlog::info("Successfully added file to shared memory: {}", path);

      auto data = utils::create_success_response(
          {"path", "size", "created"}, path,
          static_cast<Json::UInt64>(content.size()), true);

      response.send(Pistache::Http::Code::Ok, data.toStyledString());

    } catch (const std::exception &e) {
      spdlog::error("Exception in file creation: {}", e.what());
      auto error_response =
          utils::create_error_response("Internal server error");
      response.send(Pistache::Http::Code::Internal_Server_Error,
                    error_response.toStyledString());
    }
  }

  void handleFileRead(const Pistache::Rest::Request &request,
                      Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);

    try {
      auto path_param = request.query().get("path").value();
      if (path_param.empty()) {
        auto error_response =
            utils::create_error_response("Path parameter is required");
        response.send(Pistache::Http::Code::Bad_Request,
                      error_response.toStyledString());
        return;
      }

      auto &search =
          owl::instance::VFSInstance<EmbeddedModel>::getInstance().get_search();
      const std::string &content = search.getFileContentImpl(path_param);

      auto data = utils::create_success_response(
          {"path", "content", "size"}, path_param, content,
          static_cast<Json::UInt64>(content.size()));

      response.send(Pistache::Http::Code::Ok, data.toStyledString());

    } catch (const std::exception &e) {
      spdlog::error("Exception in file read: {}", e.what());
      auto error_response = utils::create_error_response("File not found");
      response.send(Pistache::Http::Code::Not_Found,
                    error_response.toStyledString());
    }
  }

  void handleSemanticSearch(const Pistache::Rest::Request &request,
                            Pistache::Http::ResponseWriter response) {
    addCorsHeaders(response);

    try {
      auto body = request.body();
      Json::Value json;
      Json::Reader reader;
      if (!reader.parse(body, json)) {
        auto error_response = utils::create_error_response("Invalid JSON");
        response.send(Pistache::Http::Code::Bad_Request,
                      error_response.toStyledString());
        return;
      }

      if (!json.isMember("query")) {
        auto error_response = utils::create_error_response("Missing 'query'");
        response.send(Pistache::Http::Code::Bad_Request,
                      error_response.toStyledString());
        return;
      }

      const std::string query = json["query"].asString();
      int limit = json.get("limit", 5).asInt();

      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
      auto results = vfs.get_search().semanticSearchImpl(query, limit);

      Json::Value resultsJson(Json::arrayValue);
      for (const auto &[path, score] : results) {
        Json::Value resultJson;
        resultJson["path"] = path;
        resultJson["score"] = score;
        resultsJson.append(resultJson);
      }

      auto response_data = utils::create_success_response(
          {"query", "results", "count"}, query, resultsJson,
          static_cast<int>(results.size()));

      response.send(Pistache::Http::Code::Ok, response_data.toStyledString());

    } catch (const std::exception &e) {
      spdlog::error("Exception in semantic search: {}", e.what());
      auto error_response =
          utils::create_error_response("Internal server error");
      response.send(Pistache::Http::Code::Internal_Server_Error,
                    error_response.toStyledString());
    }
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