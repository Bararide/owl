#ifndef VECTORFS_PISTACHE_API_HPP
#define VECTORFS_PISTACHE_API_HPP

#include "publisher.hpp"
#include "requests.hpp"
#include "responses.hpp"
#include "subscriber.hpp"
#include "validate.hpp"
#include <condition_variable>
#include <future>
#include <mutex>
#include <random>
#include <unordered_map>

namespace owl::api {

template <typename EmbeddedModel> class VectorFSApi {
private:
  struct PendingRequest {
    std::promise<nlohmann::json> promise;
    std::chrono::steady_clock::time_point timestamp;
  };

  std::unordered_map<std::string, PendingRequest> pending_requests_;
  std::mutex requests_mutex_;
  std::thread cleanup_thread_;
  std::atomic<bool> running_{false};

public:
  VectorFSApi()
      : httpEndpoint(std::make_unique<Pistache::Http::Endpoint>(
            Pistache::Address("0.0.0.0", 9999))),
        publisher_(
            std::make_unique<pub::MessagePublisher>("tcp://localhost:5555")),
        subscriber_(
            std::make_unique<sub::MessageSubscriber>("tcp://localhost:5556")) {}

  void init() {
    spdlog::info("Initializing Pistache API...");

    initSubscriber();
    startCleanupThread();

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
    running_ = false;

    if (cleanup_thread_.joinable()) {
      cleanup_thread_.join();
    }

    subscriber_->stop();
    httpEndpoint->shutdown();
  }

private:
  void initSubscriber() {
    subscriber_->registerHandler(
        [this](const nlohmann::json &msg) { handleResponse(msg); });

    subscriber_->start();
  }

  void startCleanupThread() {
    running_ = true;
    cleanup_thread_ = std::thread([this]() {
      while (running_) {
        cleanupExpiredRequests();
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
  }

  void cleanupExpiredRequests() {
    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto now = std::chrono::steady_clock::now();

    auto it = pending_requests_.begin();
    while (it != pending_requests_.end()) {
      auto duration = std::chrono::duration_cast<std::chrono::seconds>(
          now - it->second.timestamp);

      if (duration.count() > 30) {
        it->second.promise.set_value(
            {{"success", false}, {"error", "Request timeout"}});
        it = pending_requests_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void handleResponse(const nlohmann::json &response) {
    std::string request_id = response.value("request_id", "");

    if (request_id.empty()) {
      spdlog::warn("Received response without request_id");
      return;
    }

    std::lock_guard<std::mutex> lock(requests_mutex_);
    auto it = pending_requests_.find(request_id);

    if (it != pending_requests_.end()) {
      it->second.promise.set_value(response);
      pending_requests_.erase(it);
      spdlog::debug("Response handled for request: {}", request_id);
    } else {
      spdlog::warn("No pending request found for id: {}", request_id);
    }
  }

  nlohmann::json sendRequestToVectorFS(const nlohmann::json &request) {
    try {
      std::string request_id = generate_uuid();

      nlohmann::json full_request = request;
      full_request["request_id"] = request_id;
      full_request["timestamp"] = std::time(nullptr);

      spdlog::critical("Sending request: {}", full_request.dump());

      std::promise<nlohmann::json> promise;
      auto future = promise.get_future();

      {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_requests_[request_id] = {std::move(promise),
                                         std::chrono::steady_clock::now()};
      }

      if (!publisher_->sendMessage(full_request.dump())) {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_requests_.erase(request_id);
        return {{"success", false},
                {"error", "Failed to send request to VectorFS"}};
      }

      spdlog::critical("Message sent, waiting for response...");

      auto status = future.wait_for(std::chrono::seconds(10));

      if (status == std::future_status::timeout) {
        spdlog::critical("Request timeout");
        std::lock_guard<std::mutex> lock(requests_mutex_);
        pending_requests_.erase(request_id);
        return {{"success", false}, {"error", "Request timeout"}};
      }

      auto result = future.get();
      spdlog::critical("Response received: {}", result.dump());
      return result;

    } catch (const std::exception &e) {
      spdlog::critical("Exception in sendRequestToVectorFS: {}", e.what());
      return {{"success", false},
              {"error", std::string("Exception: ") + e.what()}};
    }
  }

  std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char *hex_chars = "0123456789abcdef";
    std::string uuid;

    for (int i = 0; i < 32; ++i) {
      uuid += hex_chars[dis(gen)];
      if (i == 7 || i == 11 || i == 15 || i == 19) {
        uuid += '-';
      }
    }

    return uuid;
  }

  Json::Value convertToJsonValue(const nlohmann::json &nj) {
    Json::Value result;

    if (nj.is_object()) {
      for (auto it = nj.begin(); it != nj.end(); ++it) {
        result[it.key()] = convertToJsonValue(it.value());
      }
    } else if (nj.is_array()) {
      result = Json::Value(Json::arrayValue);
      for (const auto &item : nj) {
        result.append(convertToJsonValue(item));
      }
    } else if (nj.is_string()) {
      result = nj.get<std::string>();
    } else if (nj.is_number_integer()) {
      result = nj.get<int64_t>();
    } else if (nj.is_number_float()) {
      result = nj.get<double>();
    } else if (nj.is_boolean()) {
      result = nj.get<bool>();
    } else if (nj.is_null()) {
      result = Json::Value();
    }

    return result;
  }

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
    Routes::Get(
        router, "/container/files/refresh",
        Routes::bind(&VectorFSApi::handleContainerRebuildIndexAndFilesGet,
                     this));
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
    responses::addCorsHeaders(response);
    response.send(Pistache::Http::Code::Ok, "OK");
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

              nlohmann::json request_msg = {{"type", "get_container_files"},
                                            {"user_id", user_id},
                                            {"container_id", container_id}};

              spdlog::info("Sending request to VectorFS: {}",
                           request_msg.dump());

              auto zmq_result = sendRequestToVectorFS(request_msg);

              spdlog::info("Received from VectorFS: {}", zmq_result.dump());

              if (zmq_result.value("success", false)) {
                if (!zmq_result.contains("data")) {
                  spdlog::error("VectorFS response missing 'data' field");
                  return core::Result<nlohmann::json, std::string>::Error(
                      "No data in response");
                }

                auto data = zmq_result["data"];
                spdlog::info("Data type: {}", data.type_name());
                spdlog::info("Data value: {}", data.dump());

                return core::Result<nlohmann::json, std::string>(data);
              } else {
                spdlog::error("VectorFS error: {}",
                              zmq_result.value("error", "Unknown error"));
                return core::Result<nlohmann::json, std::string>::Error(
                    zmq_result.value("error", "Failed to get container files"));
              }
            })
            .map([](nlohmann::json data) -> Json::Value {
              return utils::create_success_response(
                  {"memory_limit", "cpu_limit"},
                  data.value("memory_limit", 100),
                  data.value("cpu_limit", 100));
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleContainerRebuildIndexAndFilesGet(
      const Pistache::Rest::Request &request,
      Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              spdlog::debug("Parsed request body: {}", json.toStyledString());
              return validate::Validator::validate<validate::Container>(json);
            })
            .and_then([this](validate::Container params) {
              auto [user_id, container_id] = params;
              spdlog::info("Getting files for container: {} for user: {}",
                           container_id, user_id);

              nlohmann::json request_msg = {{"type", "get_container_files_and_rebuild"},
                                            {"user_id", user_id},
                                            {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);
              spdlog::debug("ZeroMQ response: {}", zmq_result.dump());

              if (zmq_result.value("success", false)) {
                if (zmq_result.contains("data")) {
                  auto data = zmq_result["data"];

                  Json::Value json_result;

                  Json::Value files_array(Json::arrayValue);
                  if (data.contains("files") && data["files"].is_array()) {
                    for (const auto &file : data["files"]) {
                      Json::Value file_obj;

                      if (file.contains("name"))
                        file_obj["name"] =
                            file["name"].template get<std::string>();
                      if (file.contains("path"))
                        file_obj["path"] =
                            file["path"].template get<std::string>();
                      if (file.contains("content"))
                        file_obj["content"] =
                            file["content"].template get<std::string>();
                      if (file.contains("size"))
                        file_obj["size"] = file["size"].template get<int>();
                      if (file.contains("exists"))
                        file_obj["exists"] =
                            file["exists"].template get<bool>();
                      if (file.contains("is_directory"))
                        file_obj["is_directory"] =
                            file["is_directory"].template get<bool>();
                      if (file.contains("category"))
                        file_obj["category"] =
                            file["category"].template get<std::string>();

                      files_array.append(file_obj);
                    }
                  }

                  return core::Result<Json::Value, std::string>::Ok(
                      files_array);

                } else {
                  spdlog::error("Response missing 'data' field");
                  return core::Result<Json::Value, std::string>::Error(
                      "No data in response");
                }
              } else {
                std::string error = zmq_result.value("error", "Unknown error");
                spdlog::error("ZeroMQ error: {}", error);
                return core::Result<Json::Value, std::string>::Error(error);
              }
            })
            .map([this](Json::Value files_array) -> Json::Value {
              Json::Value data;
              data["files"] = files_array;
              data["count"] = static_cast<int>(files_array.size());

              return utils::create_success_response(
                  {"files", "count"}, files_array,
                  static_cast<int>(files_array.size()));
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
              spdlog::debug("Parsed request body: {}", json.toStyledString());
              return validate::Validator::validate<validate::Container>(json);
            })
            .and_then([this](validate::Container params) {
              auto [user_id, container_id] = params;
              spdlog::info("Getting files for container: {} for user: {}",
                           container_id, user_id);

              nlohmann::json request_msg = {{"type", "get_container_files"},
                                            {"user_id", user_id},
                                            {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);
              spdlog::debug("ZeroMQ response: {}", zmq_result.dump());

              if (zmq_result.value("success", false)) {
                if (zmq_result.contains("data")) {
                  auto data = zmq_result["data"];

                  Json::Value json_result;

                  Json::Value files_array(Json::arrayValue);
                  if (data.contains("files") && data["files"].is_array()) {
                    for (const auto &file : data["files"]) {
                      Json::Value file_obj;

                      if (file.contains("name"))
                        file_obj["name"] =
                            file["name"].template get<std::string>();
                      if (file.contains("path"))
                        file_obj["path"] =
                            file["path"].template get<std::string>();
                      if (file.contains("content"))
                        file_obj["content"] =
                            file["content"].template get<std::string>();
                      if (file.contains("size"))
                        file_obj["size"] = file["size"].template get<int>();
                      if (file.contains("exists"))
                        file_obj["exists"] =
                            file["exists"].template get<bool>();
                      if (file.contains("is_directory"))
                        file_obj["is_directory"] =
                            file["is_directory"].template get<bool>();
                      if (file.contains("category"))
                        file_obj["category"] =
                            file["category"].template get<std::string>();

                      files_array.append(file_obj);
                    }
                  }

                  return core::Result<Json::Value, std::string>::Ok(
                      files_array);

                } else {
                  spdlog::error("Response missing 'data' field");
                  return core::Result<Json::Value, std::string>::Error(
                      "No data in response");
                }
              } else {
                std::string error = zmq_result.value("error", "Unknown error");
                spdlog::error("ZeroMQ error: {}", error);
                return core::Result<Json::Value, std::string>::Error(error);
              }
            })
            .map([this](Json::Value files_array) -> Json::Value {
              Json::Value data;
              data["files"] = files_array;
              data["count"] = static_cast<int>(files_array.size());

              return utils::create_success_response(
                  {"files", "count"}, files_array,
                  static_cast<int>(files_array.size()));
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
            .and_then([this](validate::SemanticSearchInContainer params) {
              auto [query, limit, user_id, container_id] = params;

              nlohmann::json request_msg = {
                  {"type", "semantic_search_in_container"},
                  {"query", query},
                  {"limit", limit},
                  {"user_id", user_id},
                  {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                auto data = zmq_result["data"];

                Json::Value data_json = convertToJsonValue(data);
                return core::Result<Json::Value, std::string>::Ok(data_json);
              } else {
                return core::Result<Json::Value, std::string>::Error(
                    zmq_result.value("error",
                                     "Failed to perform semantic search"));
              }
            })
            .map([](Json::Value data) -> Json::Value {
              std::string query = data.get("query", "").asString();
              std::string container_id =
                  data.get("container_id", "").asString();
              Json::Value results = data["results"];
              int count = data.get("count", 0).asInt();

              return utils::create_success_response(
                  {"query", "container_id", "results", "count"}, query,
                  container_id, results, count);
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

              nlohmann::json request_msg = {{"type", "create_file"},
                                            {"path", path},
                                            {"content", content},
                                            {"user_id", user_id},
                                            {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                return core::
                    Result<std::pair<std::string, size_t>, std::string>::Ok(
                        std::make_pair(path, content.size()));
              } else {
                return core::
                    Result<std::pair<std::string, size_t>, std::string>::Error(
                        zmq_result.value("error", "Failed to create file"));
              }
            })
            .map([](std::pair<std::string, size_t> result) -> Json::Value {
              auto [path, size] = result;
              return utils::create_success_response(
                  {"path", "size", "created", "container_id", "message"}, path,
                  static_cast<Json::UInt64>(size), true,
                  "container_id_placeholder", "File created successfully");
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleContainerDelete(const Pistache::Rest::Request &request,
                             Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::DeleteContainer>(
                  json);
            })
            .and_then([this](validate::DeleteContainer params) {
              auto [user_id, container_id] = params;

              nlohmann::json request_msg = {{"type", "container_delete"},
                                            {"user_id", user_id},
                                            {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                return core::Result<
                    std::pair<std::string, std::string>,
                    std::string>::Ok(std::make_pair(container_id, user_id));
              } else {
                return core::Result<std::pair<std::string, std::string>,
                                    std::string>::
                    Error(zmq_result.value("error",
                                           "Failed to delete container"));
              }
            })
            .map([](std::pair<std::string, std::string> result) -> Json::Value {
              auto [container_id, user_id] = result;
              return utils::create_success_response(
                  {"container_id", "user_id", "status", "message"},
                  container_id, user_id, "deleted",
                  "Container deleted successfully");
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

              nlohmann::json request_msg = {{"type", "file_delete"},
                                            {"user_id", user_id},
                                            {"container_id", container_id},
                                            {"path", file_path}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                return core::Result<
                    std::tuple<std::string, std::string, std::string>,
                    std::string>::Ok(std::make_tuple(file_path, container_id,
                                                     user_id));
              } else {
                return core::Result<
                    std::tuple<std::string, std::string, std::string>,
                    std::string>::Error(zmq_result
                                            .value("error",
                                                   "Failed to delete file"));
              }
            })
            .map([](std::tuple<std::string, std::string, std::string> result)
                     -> Json::Value {
              auto [file_path, container_id, user_id] = result;
              return utils::create_success_response(
                  {"file_path", "container_id", "user_id", "status", "message"},
                  file_path, container_id, user_id, "deleted",
                  "File deleted successfully");
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleContainerCreate(const Pistache::Rest::Request &request,
                             Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .and_then([](Json::Value json) {
              return validate::Validator::validate<validate::CreateContainer>(
                  json);
            })
            .and_then([this](validate::CreateContainer params) {
              nlohmann::json request_msg = {
                  {"type", "container_create"},
                  {"container_id", params.container_id},
                  {"user_id", params.user_id},
                  {"memory_limit", params.memory_limit},
                  {"storage_quota", params.storage_quota},
                  {"file_limit", params.file_limit},
                  {"privileged", params.privileged},
                  {"env_label", params.env_label.second},
                  {"type_label", params.type_label.second},
                  {"commands", params.commands}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                return core::Result<validate::CreateContainer, std::string>::Ok(
                    params);
              } else {
                return core::Result<validate::CreateContainer, std::string>::
                    Error(zmq_result.value("error",
                                           "Failed to create container"));
              }
            })
            .map([](validate::CreateContainer params) -> Json::Value {
              return utils::create_success_response(
                  {"container_id", "status", "memory_limit", "storage_quota",
                   "file_limit", "message"},
                  params.container_id, "created",
                  static_cast<Json::UInt64>(params.memory_limit),
                  static_cast<Json::UInt64>(params.storage_quota),
                  static_cast<Json::UInt64>(params.file_limit),
                  "Container created successfully");
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
            .and_then([this](validate::ReadFileByIdBody params) {
              auto [file_id, container_id] = params;

              nlohmann::json request_msg = {{"type", "get_file_content"},
                                            {"file_id", file_id},
                                            {"container_id", container_id}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                auto data = zmq_result["data"];
                Json::Value data_json = convertToJsonValue(data);

                Json::Value result_json;
                result_json["content"] = data_json["content"];
                return core::Result<Json::Value, std::string>::Ok(result_json);
              } else {
                return core::Result<Json::Value, std::string>::Error(
                    zmq_result.value("error", "Failed to get file content"));
              }
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
            .and_then([this](validate::SemanticSearch params) {
              auto [query, limit] = params;

              nlohmann::json request_msg = {{"type", "semantic_search"},
                                            {"query", query},
                                            {"limit", limit}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                auto data = zmq_result["data"];

                Json::Value data_json = convertToJsonValue(data);
                return core::Result<Json::Value, std::string>::Ok(data_json);
              } else {
                return core::Result<Json::Value, std::string>::Error(
                    zmq_result.value("error",
                                     "Failed to perform semantic search"));
              }
            })
            .map([](Json::Value data) -> Json::Value {
              std::string query = data.get("query", "").asString();
              std::string container_id =
                  data.get("container_id", "").asString();
              Json::Value results = data["results"];
              int count = data.get("count", 0).asInt();

              return utils::create_success_response(
                  {"query", "container_id", "results", "count"}, query,
                  container_id, results, count);
            });

    response.headers().add<Pistache::Http::Header::ContentType>(
        MIME(Application, Json));
    responses::handleJsonResult(result, response);
  }

  void handleRebuild(const Pistache::Rest::Request &request,
                     Pistache::Http::ResponseWriter response) {
    auto result =
        responses::parseJsonBody(request.body())
            .map([this](Json::Value json) -> Json::Value {
              nlohmann::json request_msg = {{"type", "rebuild_index"}};

              auto zmq_result = sendRequestToVectorFS(request_msg);

              if (zmq_result.value("success", false)) {
                return utils::create_success_response(
                    {"message"}, "Rebuild completed successfully");
              } else {
                throw std::runtime_error(
                    zmq_result.value("error", "Failed to rebuild index"));
              }
            });

    responses::handleJsonResult(result, response);
  }

private:
  std::unique_ptr<Pistache::Http::Endpoint> httpEndpoint;
  Pistache::Rest::Router router;
  std::unique_ptr<pub::MessagePublisher> publisher_;
  std::unique_ptr<sub::MessageSubscriber> subscriber_;
};

} // namespace owl::api

#endif