#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "publisher.hpp"
#include "requests.hpp"
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
    Routes::Get(router, "/files/read",
                Routes::bind(&VectorFSApi::getFileById, this));
    Routes::Get(router, "/container/metrics",
                Routes::bind(&VectorFSApi::handleGetContainerMetrics, this));
    Routes::Get(router, "/container/files",
                Routes::bind(&VectorFSApi::handleContainerFilesGet, this));
    Routes::Delete(router, "/containers/delete",
                   Routes::bind(&VectorFSApi::handleContainerDelete, this));
    Routes::Delete(router, "/files/delete",
                   Routes::bind(&VectorFSApi::handleFileDelete, this));
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

  void handleGetContainerMetrics(const Pistache::Rest::Request &request,
                                 Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::Container>(json);
            })
            .and_then([this](validate::Container params) {
              auto [user_id, container_id] = params;

              validate::GetContainerMetrics metrics{};

              if (publisher_->sendContainerMetrics(user_id, container_id,
                                                   metrics)) {
                return core::Result<validate::GetContainerMetrics,
                                    std::string>::Ok(metrics);
              } else {
                return core::
                    Result<validate::GetContainerMetrics, std::string>::Error(
                        "Failed to get container metrics");
              }
            })
            .map([](validate::GetContainerMetrics result) -> Json::Value {
              auto [memory_limit, cpu_limit] = result;

              auto response_json = utils::create_success_response(
                  {"memory_limit", "cpu_limit"}, memory_limit, cpu_limit);

              return response_json;
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleFileCreate(const Pistache::Rest::Request &request,
                        Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::CreateFile>(json);
            })
            .and_then([this](validate::CreateFile params) {
              auto [path, content, user_id, container_id] = params;

              if (publisher_->sendFileCreate(path, content, user_id,
                                             container_id)) {
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
              return utils::create_success_response(
                  {"path", "size", "created", "container_id", "message"}, path,
                  static_cast<Json::UInt64>(size), true,
                  "container_id_placeholder",
                  "File creation request sent to FUSE process");
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleContainerFilesGet(const Pistache::Rest::Request &request,
                               Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::Container>(json);
            })
            .and_then([this](validate::Container params) {
              auto [user_id, container_id] = params;

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container = container_manager.get_container(container_id);

              if (!container) {
                auto &fuse_instance =
                    owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                        .get_vector_fs();
                auto fuse_container =
                    fuse_instance.get_container_adapter(container_id);

                if (fuse_container) {
                  container = fuse_container;
                }
              }

              if (!container) {
                spdlog::warn("Container not found: {}", container_id);
                return core::Result<validate::Container, std::string>::Error(
                    "Container not found: " + container_id);
              }

              if (container->getOwner() != user_id) {
                spdlog::warn("User {} doesn't have permission for container {} "
                             "owned by {}",
                             user_id, container_id, container->getOwner());
                return core::Result<validate::Container, std::string>::Error(
                    "Access denied");
              }

              return core::Result<validate::Container, std::string>::Ok(params);
            })
            .and_then([this](validate::Container params) {
              auto [user_id, container_id] = params;

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container =
                  vfs.get_vector_fs().get_unified_container(container_id);
              if (!container) {
                return core::Result<Json::Value, std::string>::Error(
                    "Container not found after validation: " + container_id);
              }

              auto files = container->listFiles("/");
              Json::Value filesArray(Json::arrayValue);

              for (const auto &file_name : files) {
                std::string file_path = file_name;
                if (file_path[0] != '/') {
                  file_path = "/" + file_path;
                }

                Json::Value fileInfo;
                fileInfo["name"] = file_name;
                fileInfo["path"] = file_path;

                std::string content = container->getFileContent(file_path);
                fileInfo["content"] = content;
                fileInfo["size"] = static_cast<Json::UInt64>(content.size());
                fileInfo["exists"] = container->fileExists(file_path);
                fileInfo["is_directory"] = container->isDirectory(file_path);
                fileInfo["category"] = container->classifyFile(file_path);

                filesArray.append(fileInfo);
              }
              
              return core::Result<Json::Value, std::string>::Ok(filesArray);
            })
            .map([this](Json::Value filesArray) -> Json::Value {
              return utils::create_success_response(
                  {"files", "count"}, filesArray,
                  static_cast<int>(filesArray.size()));
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleContainerDelete(const Pistache::Rest::Request &request,
                             Pistache::Http::ResponseWriter response) {
    spdlog::info("=== Container Deletion Request ===");
    spdlog::info("Client IP: {}", request.address().host());
    spdlog::info("URL: {}", request.resource());

    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::DeleteContainer>(
                  json);
            })
            .and_then([&request](validate::DeleteContainer params) {
              auto [user_id, container_id] = params;

              if (container_id.empty()) {
                return core::Result<validate::DeleteContainer, std::string>::
                    Error("Container ID is required");
              }

              spdlog::info("Deleting container: {} for user: {}", container_id,
                           user_id);
              return core::Result<validate::DeleteContainer, std::string>::Ok(
                  params);
            })
            .and_then([this](validate::DeleteContainer params) {
              auto [user_id, container_id] = params;

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container = container_manager.get_container(container_id);
              if (!container) {
                spdlog::warn("Container not found: {}", container_id);
                return core::Result<validate::DeleteContainer, std::string>::
                    Error("Container not found: " + container_id);
              }

              if (container->getOwner() != user_id) {
                spdlog::warn("User {} does not have permission to delete "
                             "container {} owned by {}",
                             user_id, container_id, container->getOwner());
                return core::Result<validate::DeleteContainer, std::string>::
                    Error("Access denied: you don't have permission to delete "
                          "this container");
              }

              return core::Result<validate::DeleteContainer, std::string>::Ok(
                  params);
            })
            .and_then([this](validate::DeleteContainer params) {
              auto [user_id, container_id] = params;

              if (publisher_->sendContainerDelete(container_id, user_id)) {

                auto &vfs =
                    owl::instance::VFSInstance<EmbeddedModel>::getInstance();
                auto &state = vfs.get_state();
                auto &container_manager = state.getContainerManager();

                bool unregistered =
                    container_manager.unregister_container(container_id);
                if (unregistered) {
                  spdlog::info(
                      "Container unregistered from container manager: {}",
                      container_id);
                } else {
                  spdlog::warn("Container not found in container manager "
                               "during unregistration: {}",
                               container_id);
                }

                return core::Result<
                    std::pair<std::string, std::string>,
                    std::string>::Ok(std::make_pair(container_id, user_id));
              } else {
                return core::Result<std::pair<std::string, std::string>,
                                    std::string>::
                    Error(
                        "Failed to send container deletion message via ZeroMQ");
              }
            })
            .map([](std::pair<std::string, std::string> result) -> Json::Value {
              auto [container_id, user_id] = result;

              return utils::create_success_response(
                  {"container_id", "user_id", "status", "message"},
                  container_id, user_id, "deletion_pending",
                  "Container deletion request sent to FUSE process");
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleFileDelete(const Pistache::Rest::Request &request,
                        Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::DeleteFile>(json);
            })
            .and_then([this](validate::DeleteFile params) {
              auto [user_id, container_id, file_path] = params;

              if (file_path.empty()) {
                return core::Result<validate::DeleteFile, std::string>::Error(
                    "File path is required");
              }

              if (container_id.empty()) {
                return core::Result<validate::DeleteFile, std::string>::Error(
                    "Container ID is required");
              }

              auto &vfs =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs.get_state();
              auto &container_manager = state.getContainerManager();

              auto container = container_manager.get_container(container_id);
              if (!container) {
                spdlog::warn("Container not found: {}", container_id);
                return core::Result<validate::DeleteFile, std::string>::Error(
                    "Container not found: " + container_id);
              }

              if (container->getOwner() != user_id) {
                spdlog::warn("User {} does not have permission to delete files "
                             "from container {} owned by {}",
                             user_id, container_id, container->getOwner());
                return core::Result<validate::DeleteFile, std::string>::Error(
                    "Access denied: you don't have permission to delete files "
                    "from this container");
              }

              if (!container->fileExists(file_path)) {
                spdlog::warn("File not found in container: {}", file_path);
                return core::Result<validate::DeleteFile, std::string>::Error(
                    "File not found: " + file_path);
              }

              return core::Result<validate::DeleteFile, std::string>::Ok(
                  params);
            })
            .and_then([this](validate::DeleteFile params) {
              auto [user_id, container_id, file_path] = params;

              if (publisher_->sendFileDelete(file_path, user_id,
                                             container_id)) {

                auto &vfs =
                    owl::instance::VFSInstance<EmbeddedModel>::getInstance();
                auto &state = vfs.get_state();
                auto &container_manager = state.getContainerManager();

                auto container = container_manager.get_container(container_id);
                if (container) {
                  bool deleted = container->removeFile(file_path);
                  if (deleted) {
                    spdlog::info("File successfully deleted from container: {}",
                                 file_path);
                  } else {
                    spdlog::warn("Failed to delete file from container: {}",
                                 file_path);
                  }
                }

                return core::Result<
                    std::tuple<std::string, std::string, std::string>,
                    std::string>::Ok(std::make_tuple(file_path, container_id,
                                                     user_id));
              } else {
                return core::Result<
                    std::tuple<std::string, std::string, std::string>,
                    std::string>::
                    Error("Failed to send file deletion message via ZeroMQ");
              }
            })
            .map([](std::tuple<std::string, std::string, std::string> result)
                     -> Json::Value {
              auto [file_path, container_id, user_id] = result;
              spdlog::info("File deletion request processed successfully: "
                           "{} from container {} for user: {}",
                           file_path, container_id, user_id);

              return utils::create_success_response(
                  {"file_path", "container_id", "user_id", "status", "message"},
                  file_path, container_id, user_id, "deleted",
                  "File deletion request sent to FUSE process");
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
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

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
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

              auto content = container->getFileContent(file_id);

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

              auto results = container->semanticSearch(query, limit);

              spdlog::debug("Semantic search found {} results", results.size());

              Json::Value resultsJson(Json::arrayValue);
              for (const auto &[file_path, score] : results) {
                if (score < 1.0) {
                  Json::Value resultJson;
                  resultJson["path"] = file_path;
                  resultJson["score"] = score;
                  resultsJson.append(resultJson);
                }
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