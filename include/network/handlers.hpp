#ifndef VECTORFS_NETWORK_HANDLERS_HPP
#define VECTORFS_NETWORK_HANDLERS_HPP

#include "vectorfs.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/infrastructure/notification.hpp"
#include "core/utils/error.hpp"
#include "core/utils/success.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <drogon/HttpAppFramework.h>

namespace vfs::network {
namespace handler {

using HttpSuccess = core::utils::Success<Json::Value>;
using HttpError = core::utils::Error;

inline auto &get_success_notification() {
  static auto notif =
      core::make_notification<HttpSuccess>([](const HttpSuccess &success) {
        spdlog::info("HTTP Success: {}", success.serialize());
      });
  return notif;
}

inline auto &get_error_notification() {
  static auto notif =
      core::make_notification<HttpError>([](const HttpError &error) {
        spdlog::error("HTTP Error: {}", error.serialize());
      });
  return notif;
}

inline void notify_success(const Json::Value &data) {
  get_success_notification()(HttpSuccess{data});
}

inline void notify_error(const std::string &message) {
  get_error_notification()(HttpError{message});
}

using HttpHandler = std::function<void(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&)>;

auto create_handler(auto handler_logic) {
  return [handler_logic = std::move(handler_logic)](
             const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    try {
      std::vector<std::string> path;
      auto result = handler_logic(req, path);
      notify_success(result);

      Json::Value responseJson;
      responseJson["status"] = "success";
      responseJson["data"] = result;

      auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
      callback(resp);

    } catch (const std::exception &e) {
      notify_error(e.what());

      Json::Value errorJson;
      errorJson["status"] = "error";
      errorJson["error"] = e.what();

      auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(drogon::k500InternalServerError);
      callback(resp);
    }
  };
}

auto create_root_handler() {
  return create_handler(
      [](const drogon::HttpRequestPtr &req, const std::vector<std::string> &) {
        Json::Value response;
        response["message"] = "Hello, World!";
        return response;
      });
}

auto create_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    try {
      auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
      auto json = req->getJsonObject();

      if (!json || !json->isMember("path") || !json->isMember("content")) {
        throw std::runtime_error("Missing path or content");
      }

      std::string path = (*json)["path"].asString();
      std::string content = (*json)["content"].asString();

      struct fuse_file_info fi {};
      auto result = vfs.create(path.c_str(), 0644, &fi);
      if (result != 0) {
        throw std::runtime_error("Failed to create file");
      }

      result = vfs.write(path.c_str(), content.c_str(), content.size(), 0, &fi);
      if (result < 0) {
        throw std::runtime_error("Failed to write content");
      }

      struct stat st {};
      vfs.getattr(path.c_str(), &st, nullptr);

      Json::Value responseJson;
      responseJson["status"] = "success";

      Json::Value data;
      data["path"] = path;
      data["size"] = static_cast<Json::UInt64>(st.st_size);
      data["created"] = true;

      responseJson["data"] = data;

      auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
      callback(resp);

    } catch (const std::exception &e) {
      Json::Value errorJson;
      errorJson["status"] = "error";
      errorJson["error"] = e.what();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(drogon::k500InternalServerError);
      callback(resp);
    }
  };
}

auto read_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    try {
      spdlog::info("=== READ FILE HANDLER STARTED ===");

      auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

      auto params = req->getParameters();
      spdlog::info("Request parameters:");
      for (const auto &[key, value] : params) {
        spdlog::info("  {} = {}", key, value);
      }

      auto path_param = req->getParameter("path");
      spdlog::info("Path parameter: '{}'", path_param);

      if (path_param.empty()) {
        spdlog::error("Path parameter is empty");
        throw std::runtime_error("Path parameter is required");
      }

      std::string file_path = path_param;
      spdlog::info("Processing file path: '{}'", file_path);

      auto &virtual_files =
          vfs.get_virtual_files();

      auto it = virtual_files.find(file_path);
      if (it == virtual_files.end()) {
        spdlog::error("File not found in virtual_files: {}", file_path);
        throw std::runtime_error("File not found: " + file_path);
      }

      const auto &file_info = it->second;
      spdlog::info("File found, size: {}, content length: {}", file_info.size,
                   file_info.content.size());

      if (S_ISDIR(file_info.mode)) {
        spdlog::error("Path is a directory, not a file: {}", file_path);
        throw std::runtime_error("Path is a directory: " + file_path);
      }

      Json::Value responseJson;
      responseJson["status"] = "success";
      Json::Value data;
      data["path"] = file_path;
      data["content"] = file_info.content;
      data["size"] = static_cast<Json::UInt64>(file_info.content.size());

      responseJson["data"] = data;

      spdlog::info("Successfully read file: {}, content size: {}", file_path,
                   file_info.content.size());
      spdlog::info("Content preview: '{}'",
                   file_info.content.substr(
                       0, std::min(100, (int)file_info.content.size())));
      spdlog::info("=== READ FILE HANDLER COMPLETED ===");

      auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
      callback(resp);

    } catch (const std::exception &e) {
      spdlog::error("Exception in read_file_handler: {}", e.what());
      Json::Value errorJson;
      errorJson["status"] = "error";
      errorJson["error"] = e.what();
      auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(drogon::k404NotFound);
      callback(resp);
    }
  };
}

auto semantic_search_handler() {
  return create_handler(
      [](const drogon::HttpRequestPtr &req, const std::vector<std::string> &) {
        auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
        auto json = req->getJsonObject();

        if (!json || !json->isMember("query")) {
          throw std::runtime_error("Missing query parameter");
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

        Json::Value responseJson;
        responseJson["query"] = query;
        responseJson["results"] = resultsJson;
        responseJson["count"] = static_cast<int>(results.size());

        return responseJson;
      });
}

auto rebuild_handler() {
  return create_handler(
      [](const drogon::HttpRequestPtr &req, const std::vector<std::string> &) {
        Json::Value response;
        response["message"] = "Rebuild completed";
        return response;
      });
}

std::map<std::string, HttpHandler> handlers = {
    {"/", create_root_handler()},
    {"/files/create", create_file_handler()},
    {"/files/read", read_file_handler()},
    {"/semantic", semantic_search_handler()},
    {"/rebuild", rebuild_handler()}};
} // namespace handler
} // namespace vfs::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP