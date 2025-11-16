#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "publisher.hpp"
#include "responses.hpp"
#include "validate.hpp"

namespace owl::api {

template <typename EmbeddedModel> class VectorFSApi {
public:
  VectorFSApi()
      : httpEndpoint(std::make_unique<Pistache::Http::Endpoint>(
            Pistache::Address("0.0.0.0", 9999))),
        publisher_(std::make_unique<pub::MessagePublisher>()) {}

  void init() {
    spdlog::info("Initializing Pistache API...");

    auto opts = Pistache::Http::Endpoint::options()
                    .threads(std::thread::hardware_concurrency())
                    .flags(Pistache::Tcp::Options::ReuseAddr)
                    .maxRequestSize(10 * 1024 * 1024);

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
    // Routes::Get(router, "/files/read",
    //             Routes::bind(&VectorFSApi::handleFileRead, this));
    Routes::Get(router, "/files/read",
                Routes::bind(&VectorFSApi::getFileById, this));
    Routes::Delete(router, "/containers/:container_id",
                   Routes::bind(&VectorFSApi::handleContainerDelete, this));
    Routes::Post(router, "/containers/create",
                 Routes::bind(&VectorFSApi::handleContainerCreate, this));
    Routes::Post(router, "/semantic",
                 Routes::bind(&VectorFSApi::handleSemanticSearch, this));
    Routes::Post(
        router, "/containers/semantic",
        Routes::bind(&VectorFSApi::handleSemanticSearchInContainer, this));
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
              return validate::Validator::validate<validate::CreateFile>(json);
            })
            .and_then([this](validate::CreateFile params) {
              auto [path, content, user_id, container_id] = params;

              spdlog::info("File path: {}", path);
              spdlog::info("Content length: {} bytes", content.size());
              spdlog::info("Content: {}", content);
              spdlog::info("Target container: {}", container_id);
              spdlog::info("User: {}", user_id);

              if (publisher_->sendFileCreate(path, content, user_id,
                                             container_id)) {
                spdlog::info("File creation message sent via ZeroMQ: {}", path);
                return core::
                    Result<std::pair<std::string, size_t>, std::string>::Ok(
                        std::make_pair(path, content.size()));
              } else {
                return core::
                    Result<std::pair<std::string, size_t>, std::string>::Error(
                        "Failed to send file creation message");
              }
            })
            .map([](std::pair<std::string, size_t> result) -> Json::Value {
              auto [path, size] = result;
              spdlog::info("File creation request sent: {}", path);
              return utils::create_success_response(
                  {"path", "size", "created", "container_id", "message"}, path,
                  static_cast<Json::UInt64>(size), true,
                  "container_id_placeholder",
                  "File creation request sent to FUSE process");
            });

    responses::handleJsonResult(result, response);
  }

  void handleContainerDelete(const Pistache::Rest::Request &request,
                             Pistache::Http::ResponseWriter response) {
    spdlog::info("=== Container Deletion Request ===");
    spdlog::info("Client IP: {}", request.address().host());

    try {
      auto container_id = request.param(":container_id").as<std::string>();

      if (container_id.empty()) {
        responses::sendError(response, "Container ID is required");
        return;
      }

      spdlog::info("Deleting container: {}", container_id);

      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance();
      auto &state = vfs.get_state();
      auto &container_manager = state.getContainerManager();

      auto container = container_manager.get_container(container_id);
      if (!container) {
        spdlog::warn("Container not found: {}", container_id);
        responses::sendNotFound(response,
                                "Container not found: " + container_id);
        return;
      }

      if (publisher_->sendContainerDelete(container_id)) {
        spdlog::info("Container deletion message sent via ZeroMQ: {}",
                     container_id);

        bool unregistered =
            container_manager.unregister_container(container_id);
        if (unregistered) {
          spdlog::info("Container unregistered from container manager: {}",
                       container_id);
        }

        auto response_data = utils::create_success_response(
            {"container_id", "status", "message"}, container_id, "deleted",
            "Container deletion request sent to FUSE process");

        responses::sendSuccess(response, response_data);
      } else {
        spdlog::error("Failed to send container deletion message: {}",
                      container_id);
        responses::sendError(response,
                             "Failed to send container deletion message");
      }

    } catch (const std::exception &e) {
      spdlog::error("Error in handleContainerDelete: {}", e.what());
      responses::sendError(response,
                           std::string("Internal server error: ") + e.what());
    }
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

  void handleContainerCreate(const Pistache::Rest::Request &request,
                             Pistache::Http::ResponseWriter response) {
    spdlog::info("=== Container Creation Request ===");
    spdlog::info("Client IP: {}", request.address().host());

    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::CreateContainer>(
                  json);
            })
            .map([this](validate::CreateContainer params) -> Json::Value {
              if (publisher_->sendContainerCreate(
                      params.container_id, params.user_id, params.memory_limit,
                      params.storage_quota, params.file_limit,
                      params.privileged, params.env_label.second,
                      params.type_label.second, params.commands)) {

                spdlog::info("Container creation message sent via ZeroMQ: {}",
                             params.container_id);

                return utils::create_success_response(
                    {"container_id", "status", "memory_limit", "storage_quota",
                     "file_limit", "message"},
                    params.container_id, "pending",
                    static_cast<Json::UInt64>(params.memory_limit),
                    static_cast<Json::UInt64>(params.storage_quota),
                    static_cast<Json::UInt64>(params.file_limit),
                    "Container creation request sent to FUSE process");
              } else {
                throw std::runtime_error(
                    "Failed to send container creation message");
              }
            });

    responses::handleJsonResult(result, response);
  }

  void getFileById(const Pistache::Rest::Request &request,
                   Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::ReadFileByIdBody>(
                  json);
            })
            .map([this](validate::ReadFileByIdBody params) -> Json::Value {
              auto [file_id, container_id] = params;

              spdlog::info("file_id: {}", file_id);
              spdlog::info("container_id, {}", container_id);

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container = container_manager.get_container(container_id);

              if (!container) {
                throw std::runtime_error("Container not found: " +
                                         container_id);
              }

              auto content = container->get_file_content(file_id);

              return content;

              Json::Value result_json;
              result_json["content"] = content;

              return utils::create_success_response({"content"}, content);
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void
  handleSemanticSearchInContainer(const Pistache::Rest::Request &request,
                                  Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<
                  validate::SemanticSearchInContainer>(json);
            })
            .map([this](validate::SemanticSearchInContainer params)
                     -> Json::Value {
              auto [query, limit, user_id, container_id] = params;

              spdlog::debug("Semantic search in container {}: '{}'",
                            container_id, query);

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();

              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container = container_manager.get_container(container_id);
              if (!container) {
                spdlog::error("container with id: {} not found", container_id);
                throw std::runtime_error("Container not found: " +
                                         container_id);
              }

              auto results = container->semantic_search(query, limit);

              spdlog::debug("Semantic search found {} results", results.size());

              Json::Value resultsJson(Json::arrayValue);
              for (const auto &[file_path, score] : results) {
                Json::Value resultJson;
                resultJson["path"] = file_path;
                resultJson["score"] = score;
                resultsJson.append(resultJson);
              }

              return utils::create_success_response(
                  {"query", "container_id", "results", "count"}, query,
                  container_id, resultsJson, static_cast<int>(results.size()));
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleSemanticSearch(const Pistache::Rest::Request &request,
                            Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::SemanticSearch>(
                  json);
            })
            .map([this](validate::SemanticSearch params) -> Json::Value {
              auto [query, limit] = params;

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
              auto results = vfs.getSearch().semanticSearchImpl(query, limit);

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
  std::unique_ptr<pub::MessagePublisher> publisher_;
};

} // namespace owl::api

#endif // VECTORFS_PISTACHE_API_HPP