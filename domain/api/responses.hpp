#ifndef OWL_API_RESPONSES_HPP
#define OWL_API_RESPONSES_HPP

#include "infrastructure/result.hpp"
#include "instance/instance.hpp"
#include "shared_memory/shared_memory.hpp"
#include "utils/http_helpers.hpp"

#include <functional>
#include <pistache/http.h>
#include <pistache/router.h>
#include <string>

namespace owl::api::responses {

inline void addCorsHeaders(Pistache::Http::ResponseWriter &response) {
  response.headers().add<Pistache::Http::Header::AccessControlAllowOrigin>("*");
  response.headers().add<Pistache::Http::Header::AccessControlAllowMethods>(
      "GET, POST, PUT, DELETE, OPTIONS");
  response.headers().add<Pistache::Http::Header::AccessControlAllowHeaders>(
      "Content-Type, Authorization");
}

inline void sendSuccess(Pistache::Http::ResponseWriter &response,
                        const Json::Value &data,
                        Pistache::Http::Code code = Pistache::Http::Code::Ok) {
  addCorsHeaders(response);
  response.send(code, data.toStyledString());
}

inline void
sendError(Pistache::Http::ResponseWriter &response, const std::string &message,
          Pistache::Http::Code code = Pistache::Http::Code::Bad_Request) {
  addCorsHeaders(response);
  auto error_response = utils::create_error_response(message);
  response.send(code, error_response.toStyledString());
}

inline void
sendInternalError(Pistache::Http::ResponseWriter &response,
                  const std::string &message = "Internal server error") {
  sendError(response, message, Pistache::Http::Code::Internal_Server_Error);
}

inline void sendNotFound(Pistache::Http::ResponseWriter &response,
                         const std::string &message = "Not found") {
  sendError(response, message, Pistache::Http::Code::Not_Found);
}

template <typename T, typename E, typename SuccessHandler,
          typename ErrorHandler>
inline void handleResult(core::Result<T, E> &result,
                         Pistache::Http::ResponseWriter &response,
                         SuccessHandler onSuccess, ErrorHandler onError) {
  result.handle(onSuccess, onError);
}

template <typename T, typename E, typename SuccessHandler>
inline void handleResultWithDefault(core::Result<T, E> &result,
                                    Pistache::Http::ResponseWriter &response,
                                    SuccessHandler onSuccess) {
  result.handle(onSuccess,
                [&response](E &error) { sendError(response, error); });
}

template <typename SuccessHandler>
inline void
handleJsonResultWithHandler(core::Result<Json::Value, std::string> &result,
                            Pistache::Http::ResponseWriter &response,
                            SuccessHandler onSuccess) {
  handleResultWithDefault(result, response, onSuccess);
}

inline void handleJsonResult(core::Result<Json::Value, std::string> &result,
                             Pistache::Http::ResponseWriter &response) {
  handleResultWithDefault(result, response, [&response](Json::Value &data) {
    sendSuccess(response, data);
  });
}

inline void handleBoolResult(
    core::Result<bool, std::string> &result,
    Pistache::Http::ResponseWriter &response,
    const std::string &successMessage = "Operation completed successfully") {
  handleResultWithDefault(
      result, response, [&response, successMessage](bool &success) {
        auto data = utils::create_success_response({"message"}, successMessage);
        sendSuccess(response, data);
      });
}

inline core::Result<Json::Value, std::string>
parseJsonBody(const std::string &body) {
  Json::Value json;
  Json::Reader reader;
  if (!reader.parse(body, json)) {
    return core::Result<Json::Value, std::string>::Error("Invalid JSON");
  }
  return core::Result<Json::Value, std::string>::Ok(std::move(json));
}

inline core::Result<std::pair<std::string, std::string>, std::string>
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

inline core::Result<bool, std::string> initializeSharedMemory() {
  auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
  if (!shm_manager.initialize()) {
    return core::Result<bool, std::string>::Error(
        "Failed to initialize shared memory");
  }
  return core::Result<bool, std::string>::Ok(true);
}

template <typename EmbeddedModel>
inline core::Result<bool, std::string>
addFileToContainer(const std::string &path, const std::string &content,
                   const std::string &container_id,
                   const std::string &user_id) {
  try {
    auto &vfs_instance =
        owl::instance::VFSInstance<EmbeddedModel>::getInstance();
    auto &container_manager = vfs_instance.get_state().get_container_manager();

    auto container = container_manager.get_container(container_id);
    if (!container) {
      return core::Result<bool, std::string>::Error("Container not found: " +
                                                    container_id);
    }

    if (container->get_owner() != user_id) {
      return core::Result<bool, std::string>::Error(
          "User " + user_id + " does not have access to container " +
          container_id);
    }

    auto result = container->add_file(path, content);
    if (!result) {
      return core::Result<bool, std::string>::Error(
          "Failed to create file in container");
    }

    spdlog::info("File {} successfully created in container {}", path,
                 container_id);
    return core::Result<bool, std::string>::Ok(true);

  } catch (const std::exception &e) {
    spdlog::error("Exception in addFileToContainer: {}", e.what());
    return core::Result<bool, std::string>::Error(
        std::string("Failed to add file to container: ") + e.what());
  }
}

inline core::Result<bool, std::string> addFileToContainerViaSharedMemory(
    const std::string &path, const std::string &content,
    const std::string &container_id, const std::string &user_id) {
  try {
    auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
    if (!shm_manager.initialize()) {
      return core::Result<bool, std::string>::Error(
          "Failed to initialize shared memory");
    }

    std::string container_path = "/containers/" + container_id + path;
    if (!container_path.empty() && container_path[0] != '/') {
      container_path = "/" + container_path;
    }

    if (!shm_manager.addFile(container_path, content)) {
      return core::Result<bool, std::string>::Error(
          "Failed to add file to shared memory");
    }

    spdlog::info("File {} added to shared memory for container {}",
                 container_path, container_id);
    return core::Result<bool, std::string>::Ok(true);

  } catch (const std::exception &e) {
    spdlog::error("Exception in addFileToContainerViaSharedMemory: {}",
                  e.what());
    return core::Result<bool, std::string>::Error(
        std::string("Failed to add file via shared memory: ") + e.what());
  }
}

inline core::Result<bool, std::string>
addFileToSharedMemory(const std::string &path, const std::string &content) {
  auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
  if (!shm_manager.addFile(path, content)) {
    return core::Result<bool, std::string>::Error(
        "Failed to add file to shared memory");
  }
  return core::Result<bool, std::string>::Ok(true);
}

template <typename EmbeddedModel>
inline core::Result<std::string, std::string>
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

inline core::Result<std::string, std::string>
getPathFromQuery(const Pistache::Rest::Request &request) {
  auto path_param = request.query().get("path");
  if (!path_param || path_param->empty()) {
    return core::Result<std::string, std::string>::Error(
        "Path parameter is required");
  }
  return core::Result<std::string, std::string>::Ok(*path_param);
}

} // namespace owl::api::responses

#endif // OWL_API_RESPONSES_HPP