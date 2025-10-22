#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "responses.hpp"
#include <memory>
#include <pistache/endpoint.h>
#include <pistache/net.h>
#include <pistache/router.h>
#include <thread>

namespace owl::api {

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

  void handleCors(const Pistache::Rest::Request &request,
                  Pistache::Http::ResponseWriter response) {
    responses::addCorsHeaders(response);
    response.send(Pistache::Http::Code::Ok);
  }

  void handleRoot(const Pistache::Rest::Request &request,
                  Pistache::Http::ResponseWriter response) {
    spdlog::info("=== ROOT HANDLER CALLED ===");
    spdlog::info("Path: {}", request.resource());
    spdlog::info("Client: {}", request.address().host());

    responses::addCorsHeaders(response);
    response.send(Pistache::Http::Code::Ok, "OK");

    spdlog::info("=== ROOT HANDLER COMPLETED ===");
  }

  void handleFileCreate(const Pistache::Rest::Request &request,
                        Pistache::Http::ResponseWriter response) {
    spdlog::info("=== File Creation Request ===");
    spdlog::info("Client IP: {}", request.address().host());
    spdlog::info("URL: {}", request.resource());

    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return responses::validateFileCreateParams(json);
            })
            .and_then([](std::pair<std::string, std::string> params) {
              auto [path, content] = params;

              spdlog::info("File path: {}", path);
              spdlog::info("Content length: {} bytes", content.size());

              return responses::initializeSharedMemory()
                  .and_then([path, content]() {
                    return responses::addFileToSharedMemory(path, content);
                  })
                  .map([path, content]() -> std::pair<std::string, size_t> {
                    return {path, content.size()};
                  });
            })
            .map([](std::pair<std::string, size_t> result) -> Json::Value {
              auto [path, size] = result;
              spdlog::info("Successfully added file to shared memory: {}",
                           path);
              return utils::create_success_response(
                  {"path", "size", "created"}, path,
                  static_cast<Json::UInt64>(size), true);
            });

    responses::handleJsonResult(result, response);
  }

  void handleFileRead(const Pistache::Rest::Request &request,
                      Pistache::Http::ResponseWriter response) {
    auto path_result = responses::getPathFromQuery(request);
    if (!path_result.is_ok()) {
      responses::sendError(response, path_result.error());
      return;
    }

    auto content_result =
        responses::getFileContent<EmbeddedModel>(path_result.value());
    if (!content_result.is_ok()) {
      responses::sendNotFound(response, content_result.error());
      return;
    }

    auto result =
        content_result.map([&request](std::string content) -> Json::Value {
          auto path_param = request.query().get("path").value_or("");
          return utils::create_success_response(
              {"path", "content", "size"}, path_param, content,
              static_cast<Json::UInt64>(content.size()));
        });

    responses::handleJsonResult(result, response);
  }

  void handleSemanticSearch(const Pistache::Rest::Request &request,
                            Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return responses::validateSemanticSearchParams(json);
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

    responses::handleJsonResult(result, response);
  }

  void handleRebuild(const Pistache::Rest::Request &request,
                     Pistache::Http::ResponseWriter response) {
    auto response_data =
        utils::create_success_response({"message"}, "Rebuild completed");
    responses::sendSuccess(response, response_data);
  }

private:
  std::unique_ptr<Pistache::Http::Endpoint> httpEndpoint;
  Pistache::Rest::Router router;
};

} // namespace owl::api

#endif // VECTORFS_PISTACHE_API_HPP