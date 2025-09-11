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
        auto response =
            utils::create_success_response({"message"}, "Hello, World!");
        return utils::success_result(response);
      });
}

auto create_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto process_request = [&]() -> utils::HttpResult {
      auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
      auto json = req->getJsonObject();

      if (!json) {
        return utils::error_result("Invalid JSON");
      }

      auto validate_path = utils::validate_json_member(*json, "path");
      if (!validate_path.is_ok()) {
        return validate_path.error();
      }

      auto validate_content = utils::validate_json_member(*json, "content");
      if (!validate_content.is_ok()) {
        return validate_content.error();
      }

      std::string path = (*json)["path"].asString();
      std::string content = (*json)["content"].asString();

      struct fuse_file_info fi {};
      auto result = vfs.create(path.c_str(), 0644, &fi);
      if (result != 0) {
        return utils::error_result("Failed to create file");
      }

      result = vfs.write(path.c_str(), content.c_str(), content.size(), 0, &fi);
      if (result < 0) {
        return utils::error_result("Failed to write content");
      }

      struct stat st {};
      vfs.getattr(path.c_str(), &st, nullptr);

      Json::Value data;
      data["path"] = path;
      data["size"] = static_cast<Json::UInt64>(st.st_size);
      data["created"] = true;

      return utils::success_result(data);
    };

    auto result = process_request();
    result.match(
        [&callback](const Json::Value &data) {
          Json::Value responseJson;
          responseJson["status"] = "success";
          responseJson["data"] = data;

          auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          Json::Value errorJson;
          errorJson["status"] = "error";
          errorJson["error"] = error.what();

          auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
          resp->setStatusCode(drogon::k500InternalServerError);
          callback(resp);
        });
  };
}

auto read_file_handler() {
  return [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto process_request = [&]() -> utils::HttpResult {
      auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
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

      Json::Value data;
      data["path"] = file_path;
      data["content"] = file_info.content;
      data["size"] = static_cast<Json::UInt64>(file_info.content.size());

      return utils::success_result(data);
    };

    auto result = process_request();
    result.match(
        [&callback](const Json::Value &data) {
          Json::Value responseJson;
          responseJson["status"] = "success";
          responseJson["data"] = data;

          auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          spdlog::error("Exception in read_file_handler: {}", error.what());

          Json::Value errorJson;
          errorJson["status"] = "error";
          errorJson["error"] = error.what();

          auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
          resp->setStatusCode(drogon::k404NotFound);
          callback(resp);
        });
  };
}

auto semantic_search_handler() {
  return utils::create_handler(
      [](const drogon::HttpRequestPtr &req,
         const std::vector<std::string> &) -> utils::HttpResult {
        auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
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

        Json::Value response;
        response["query"] = query;
        response["results"] = resultsJson;
        response["count"] = static_cast<int>(results.size());

        return utils::success_result(response);
      });
}

auto rebuild_handler() {
  return utils::create_handler(
      [](const drogon::HttpRequestPtr &,
         const std::vector<std::string> &) -> utils::HttpResult {
        Json::Value response;
        response["message"] = "Rebuild completed";
        return utils::success_result(response);
      });
}

std::map<std::string, utils::HttpHandler> handlers = {
    {"/", create_root_handler()},
    {"/files/create", create_file_handler()},
    {"/files/read", read_file_handler()},
    {"/semantic", semantic_search_handler()},
    {"/rebuild", rebuild_handler()}};

} // namespace handler
} // namespace vfs::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP