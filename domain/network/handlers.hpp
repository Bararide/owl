#ifndef VECTORFS_NETWORK_HANDLERS_HPP
#define VECTORFS_NETWORK_HANDLERS_HPP

#include "utils/http_helpers.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

namespace vfs::network {
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

      auto &vfs = vfs::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
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

      struct fuse_file_info fi {};
      fi.flags = O_WRONLY;

      auto result = vfs.create(path.c_str(), 0644, &fi);
      if (result != 0) {
        spdlog::error("Failed to create file '{}', error code: {}", path,
                      result);
        return utils::error_result("Failed to create file");
      }

      result = vfs.open(path.c_str(), &fi);
      if (result != 0) {
        spdlog::error("Failed to open file '{}' for writing, error code: {}",
                      path, result);
        vfs.unlink(path.c_str());
        return utils::error_result("Failed to open file for writing");
      }

      result = vfs.write(path.c_str(), content.c_str(), content.size(), 0, &fi);

      spdlog::info("Successfully wrote {} bytes to file '{}'", content.size(),
                   path);

      struct stat st {};
      vfs.getattr(path.c_str(), &st, nullptr);

      auto data = utils::create_success_response(
          {"path", "size", "created"}, path,
          static_cast<Json::UInt64>(st.st_size), true);

      spdlog::info("File creation completed successfully:");
      spdlog::info("  Path: {}", path);
      spdlog::info("  Size: {} bytes", st.st_size);
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
      auto &vfs = vfs::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
      auto path_param = req->getParameter("path");

      if (path_param.empty()) {
        return utils::error_result("Path parameter is required");
      }

      std::string file_path = path_param;
      auto &virtual_files = vfs.get_virtual_files();
      auto it = virtual_files.find(file_path);

      if (it == virtual_files.end()) {
        return utils::error_result("File not found: " + file_path);
      }

      const auto &file_info = it->second;

      if (S_ISDIR(file_info.mode)) {
        return utils::error_result("Path is a directory: " + file_path);
      }

      auto data = utils::create_success_response(
          {"path", "content", "size"}, file_path, file_info.content,
          static_cast<Json::UInt64>(file_info.content.size()));

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
        auto &vfs = vfs::instance::VFSInstance<EmbeddedModel>::getInstance()
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

        auto results = vfs.semantic_search(query, limit);

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
} // namespace vfs::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP