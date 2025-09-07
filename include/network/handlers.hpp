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
  return create_handler([](const drogon::HttpRequestPtr &req,
                           const std::vector<std::string> &) {
    auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
    auto json = req->getJsonObject();

    if (!json || !json->isMember("content")) {
      throw std::runtime_error("Missing content");
    }

    std::string full_path = req->path();
    std::string base_path = "/files/";

    boost::urls::url_view url_view(boost::urls::parse_uri(full_path).value());
    std::string decoded_path = url_view.path();

    if (decoded_path.starts_with(base_path)) {
      decoded_path = decoded_path.substr(base_path.length());
    }

    struct fuse_file_info fi {};
    auto result = vfs.create(decoded_path.c_str(), 0644, &fi);

    if (result != 0) {
      throw std::runtime_error("Failed to create file");
    }

    const std::string content = (*json)["content"].asString();
    result = vfs.write(decoded_path.c_str(), content.c_str(), content.size(), 0,
                       &fi);

    if (result < 0) {
      throw std::runtime_error("Failed to write content");
    }

    struct stat st {};
    vfs.getattr(decoded_path.c_str(), &st, nullptr);

    Json::Value responseJson;
    responseJson["path"] = decoded_path;
    responseJson["size"] = static_cast<Json::UInt64>(st.st_size);
    responseJson["created"] = true;

    return responseJson;
  });
}

auto read_file_handler() {
  return create_handler([](const drogon::HttpRequestPtr &req,
                           const std::vector<std::string> &) {
    auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

    std::string full_path = req->path();
    std::string base_path = "/files/";

    boost::urls::url_view url_view(boost::urls::parse_uri(full_path).value());
    std::string decoded_path = url_view.path();

    if (decoded_path.starts_with(base_path)) {
      decoded_path = decoded_path.substr(base_path.length());
    }

    struct stat st {};
    if (vfs.getattr(decoded_path.c_str(), &st, nullptr) != 0) {
      throw std::runtime_error("File not found");
    }

    std::vector<char> buffer(st.st_size + 1);
    auto result =
        vfs.read(decoded_path.c_str(), buffer.data(), st.st_size, 0, nullptr);

    if (result < 0) {
      throw std::runtime_error("Failed to read file");
    }

    buffer[result] = '\0';

    Json::Value responseJson;
    responseJson["path"] = decoded_path;
    responseJson["content"] = std::string(buffer.data());
    responseJson["size"] = static_cast<Json::UInt64>(st.st_size);

    return responseJson;
  });
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
    {"/files/.*", create_file_handler()},
    {"/files/.*", read_file_handler()},
    {"/semantic", semantic_search_handler()},
    {"/rebuild", rebuild_handler()}};

} // namespace handler
} // namespace vfs::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP