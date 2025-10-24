#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "responses.hpp"
#include "validate.hpp"

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
    Routes::Post(router, "/containers/create",
                 Routes::bind(&VectorFSApi::handleContainerCreate, this));
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
              return validate::Validator::validate<validate::CreateFile>(json);
            })
            .and_then([](validate::CreateFile params) {
              auto [path, content, user_id, container_id] = params;

              spdlog::info("File path: {}", path);
              spdlog::info("Content length: {} bytes", content.size());
              spdlog::info("Target container: {}", container_id);
              spdlog::info("User: {}", user_id);

              return responses::addFileToContainer<EmbeddedModel>(
                         path, content, container_id, user_id)
                  .map([path, content]() -> std::pair<std::string, size_t> {
                    return {path, content.size()};
                  });
            })
            .map([](std::pair<std::string, size_t> result) -> Json::Value {
              auto [path, size] = result;
              spdlog::info("Successfully added file to container: {}", path);
              return utils::create_success_response(
                  {"path", "size", "created", "container_id"}, path,
                  static_cast<Json::UInt64>(size), true,
                  "container_id_placeholder");
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
              // Используем VFSInstance для создания контейнера через FUSE
              auto &vfs_instance =
                  owl::instance::VFSInstance<EmbeddedModel>::getInstance();
              auto &state = vfs_instance.get_state();
              auto &container_manager = state.get_container_manager();

              spdlog::info("Creating container via FUSE: {}",
                           params.container_id);

              auto existing_container =
                  container_manager.get_container(params.container_id);
              if (existing_container) {
                throw std::runtime_error("Container already exists: " +
                                         params.container_id);
              }

              auto container_builder = ossec::ContainerBuilder::create();
              auto container_result =
                  container_builder.with_owner(params.user_id)
                      .with_container_id(params.container_id)
                      .with_data_path(
                          "/home/bararide/my_fuse_mount/.containers/" +
                          params.container_id)
                      .with_vectorfs_namespace("default")
                      .with_supported_formats(
                          {"txt", "json", "yaml", "cpp", "py"})
                      .with_vector_search(true)
                      .with_memory_limit(params.memory_limit)
                      .with_storage_quota(params.storage_quota)
                      .with_file_limit(params.file_limit)
                      .with_label("environment", params.env_label.second)
                      .with_label("type", params.type_label.second)
                      .with_commands(params.commands)
                      .privileged(params.privileged)
                      .build();

              if (!container_result.is_ok()) {
                container_result.error().what();
              }

              spdlog::info(
                  "Container built successfully, creating PID container...");
              auto container = container_result.value();
              auto pid_container =
                  std::make_shared<ossec::PidContainer>(std::move(container));

              spdlog::info("Starting container...");
              auto start_result = pid_container->start();
              if (!start_result.is_ok()) {
                start_result.error().what();
              }

              spdlog::info("Creating container adapter...");
              auto adapter = std::make_shared<vectorfs::OssecContainerAdapter>(
                  pid_container, state.get_embedder_manager());

              spdlog::info("Initializing Markov chain...");
              adapter->initialize_markov_chain();

              spdlog::info("Registering container in container manager...");
              if (!container_manager.register_container(adapter)) {
                throw std::runtime_error(
                    "Failed to register container in container manager");
              }

              spdlog::info("Successfully created and registered container: {}",
                           params.container_id);

              auto registered_container =
                  container_manager.get_container(params.container_id);
              if (!registered_container) {
                throw std::runtime_error(
                    "Container registration verification failed");
              }

              return utils::create_success_response(
                  {"container_id", "status", "memory_limit", "storage_quota",
                   "file_limit", "fuse_path"},
                  params.container_id, "running",
                  static_cast<Json::UInt64>(params.memory_limit),
                  static_cast<Json::UInt64>(params.storage_quota),
                  static_cast<Json::UInt64>(params.file_limit),
                  "/.containers/" + params.container_id);
            });

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