#ifndef VECTORFS_NETWORK_HANDLERS_HPP
#define VECTORFS_NETWORK_HANDLERS_HPP

#include "shared_memory/shared_memory.hpp"
#include "utils/http_helpers.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

namespace owl::network {
namespace handler {

auto create_root_handler() {
  return utils::create_handler(
      [](const drogon::HttpRequestPtr &,
         const std::vector<std::string> &) -> utils::HttpResult {
        return utils::success_result(
            utils::create_success_response({"message"}, "Hello, World!"));
      });
}

template <typename EmbeddedModel> auto create_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto process_request = [&]() -> utils::HttpResult {
      spdlog::info("=== File Creation Request ===");
      spdlog::info("Client IP: {}", req->getPeerAddr().toIp());
      spdlog::info("HTTP Method: {}", req->getMethodString());
      spdlog::info("URL: {}", req->getPath());

      spdlog::info("Headers:");
      for (const auto &[key, value] : req->getHeaders()) {
        spdlog::info("  {}: {}", key, value);
      }

      auto json = req->getJsonObject();

      if (!json) {
        spdlog::warn("Invalid JSON received - no JSON object found");
        spdlog::info("Raw body: {}", req->getBody());
        return utils::error_result("Invalid JSON");
      }

      spdlog::info("Received JSON: {}", json->toStyledString());

      auto validate_path = utils::validate_json_member(*json, "path");
      if (!validate_path.is_ok()) {
        spdlog::warn("Missing or invalid 'path' field in JSON");
        return validate_path.error();
      }

      auto validate_content = utils::validate_json_member(*json, "content");
      if (!validate_content.is_ok()) {
        spdlog::warn("Missing or invalid 'content' field in JSON");
        return validate_content.error();
      }

      std::string path = (*json)["path"].asString();
      std::string content = (*json)["content"].asString();

      if (!path.empty() && path[0] != '/') {
        path = "/" + path;
        spdlog::info("Normalized path to: {}", path);
      }

      spdlog::info("File path: {}", path);
      spdlog::info("Content length: {} bytes", content.size());

      auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      if (!shm_manager.initialize()) {
        spdlog::error("Failed to initialize shared memory");
        return utils::error_result("Internal server error");
      }

      if (!shm_manager.addFile(path, content)) {
        spdlog::error("Failed to add file to shared memory: {}", path);
        return utils::error_result("Failed to create file");
      }

      spdlog::info("Successfully added file to shared memory: {}", path);

      auto data = utils::create_success_response(
          {"path", "size", "created"}, path,
          static_cast<Json::UInt64>(content.size()), true);

      spdlog::info("File creation completed successfully:");
      spdlog::info("  Path: {}", path);
      spdlog::info("  Size: {} bytes", content.size());
      spdlog::info("=============================");

      return utils::success_result(data);
    };

    auto result = process_request();
    result.match(
        [&callback](const Json::Value &data) {
          spdlog::debug("Sending success response to client");
          auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          spdlog::warn("Sending error response: {}", error.what());
          auto error_response = utils::create_error_response(error.what());
          auto resp = drogon::HttpResponse::newHttpJsonResponse(error_response);
          resp->setStatusCode(drogon::k500InternalServerError);
          callback(resp);
        });
  };
}

template <typename EmbeddedModel> auto read_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto process_request = [&]() -> utils::HttpResult {
      auto &search =
          owl::instance::VFSInstance<EmbeddedModel>::getInstance().get_search();
      auto path_param = req->getParameter("path");

      if (path_param.empty()) {
        return utils::error_result("Path parameter is required");
      }

      std::string file_path = path_param;

      const std::string &content = search.getFileContentImpl(file_path);
      auto data = utils::create_success_response(
          {"path", "content", "size"}, file_path, content,
          static_cast<Json::UInt64>(content.size()));

      return utils::success_result(data);
    };

    auto result = process_request();
    result.match(
        [&callback](const Json::Value &data) {
          auto resp = drogon::HttpResponse::newHttpJsonResponse(data);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          spdlog::error("Exception in read_file_handler: {}", error.what());

          auto error_response = utils::create_error_response(error.what());
          auto resp = drogon::HttpResponse::newHttpJsonResponse(error_response);
          resp->setStatusCode(drogon::k404NotFound);
          callback(resp);
        });
  };
}

template <typename EmbeddedModel> auto semantic_search_handler() {
  return utils::create_handler(
      [](const drogon::HttpRequestPtr &req,
         const std::vector<std::string> &) -> utils::HttpResult {
        auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                        .get_vector_fs();
        auto json = req->getJsonObject();

        if (!json) {
          return utils::error_result("Invalid JSON");
        }

        auto validate_query = utils::validate_json_member(*json, "query");
        if (!validate_query.is_ok()) {
          return validate_query.error();
        }

        const std::string query = (*json)["query"].asString();
        int limit = json->get("limit", 5).asInt();

        auto results = vfs.get_search().semanticSearchImpl(query, limit);

        Json::Value resultsJson(Json::arrayValue);
        for (const auto &[path, score] : results) {
          Json::Value resultJson;
          resultJson["path"] = path;
          resultJson["score"] = score;
          resultsJson.append(resultJson);
        }

        auto response = utils::create_success_response(
            {"query", "results", "count"}, query, resultsJson,
            static_cast<int>(results.size()));

        return utils::success_result(response);
      });
}

auto rebuild_handler() {
  return utils::create_handler(
      [](const drogon::HttpRequestPtr &,
         const std::vector<std::string> &) -> utils::HttpResult {
        auto response =
            utils::create_success_response({"message"}, "Rebuild completed");
        return utils::success_result(response);
      });
}

} // namespace handler
} // namespace owl::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP