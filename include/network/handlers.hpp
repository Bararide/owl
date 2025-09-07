#ifndef VECTORFS_NETWORK_HANDLERS_HPP
#define VECTORFS_NETWORK_HANDLERS_HPP

#include "vectorfs.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <drogon/HttpAppFramework.h>

namespace vfs::network {
namespace handler {

std::map<std::string,
         std::function<void(
             const drogon::HttpRequestPtr &,
             std::function<void(const drogon::HttpResponsePtr &)> &&,
            //  const core::Notification<core::Result<core::utils::Success>> &,
             const std::vector<std::string> &)>>
    handlers = {
        {"/",
         [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            // const core::Notification<core::Result<core::utils::Success>>
            //     &locator,
            const std::vector<std::string> &path) {
           auto resp = drogon::HttpResponse::newHttpResponse();
           resp->setBody("Hello, World!");
           callback(resp);
         }},

        {"/files/.*",
         [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::vector<std::string> &path) {
           try {
             auto &vfs =
                 vfs::instance::VFSInstance::getInstance().get_vector_fs();
             auto json = req->getJsonObject();

             if (!json || !json->isMember("content")) {
               Json::Value errorJson;
               errorJson["error"] = "Missing content";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k400BadRequest);
               callback(resp);
               return;
             }

             std::string full_path = req->path();
             std::string base_path = "/files/";
             std::string file_path = full_path.substr(base_path.length());

             boost::urls::url_view url_view(
                 boost::urls::parse_uri(full_path).value());
             std::string decoded_path = url_view.path();
             decoded_path = decoded_path.substr(base_path.length());

             struct fuse_file_info fi {};
             auto result = vfs.create(decoded_path.c_str(), 0644, &fi);

             if (result != 0) {
               Json::Value errorJson;
               errorJson["error"] = "Failed to create file";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k500InternalServerError);
               callback(resp);
               return;
             }

             const std::string content = (*json)["content"].asString();
             result = vfs.write(decoded_path.c_str(), content.c_str(),
                                content.size(), 0, &fi);

             if (result < 0) {
               Json::Value errorJson;
               errorJson["error"] = "Failed to write content";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k500InternalServerError);
               callback(resp);
               return;
             }

             struct stat st {};
             vfs.getattr(decoded_path.c_str(), &st, nullptr);

             Json::Value responseJson;
             responseJson["path"] = decoded_path;
             responseJson["size"] = static_cast<Json::UInt64>(st.st_size);
             responseJson["created"] = true;

             auto resp =
                 drogon::HttpResponse::newHttpJsonResponse(responseJson);
             resp->setStatusCode(drogon::k201Created);
             callback(resp);

           } catch (const std::exception &e) {
             Json::Value errorJson;
             errorJson["error"] = e.what();
             auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
             resp->setStatusCode(drogon::k500InternalServerError);
             callback(resp);
           }
         }},

        {"/files/.*",
         [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::vector<std::string> &path) {
           try {
             auto &vfs =
                 vfs::instance::VFSInstance::getInstance().get_vector_fs();

             std::string full_path = req->path();
             std::string base_path = "/files/";
             std::string file_path = full_path.substr(base_path.length());

             boost::urls::url_view url_view(
                 boost::urls::parse_uri(full_path).value());
             std::string decoded_path = url_view.path();
             decoded_path = decoded_path.substr(base_path.length());

             struct stat st {};
             if (vfs.getattr(decoded_path.c_str(), &st, nullptr) != 0) {
               Json::Value errorJson;
               errorJson["error"] = "File not found";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k404NotFound);
               callback(resp);
               return;
             }

             std::vector<char> buffer(st.st_size + 1);
             auto result = vfs.read(decoded_path.c_str(), buffer.data(),
                                    st.st_size, 0, nullptr);

             if (result < 0) {
               Json::Value errorJson;
               errorJson["error"] = "Failed to read file";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k500InternalServerError);
               callback(resp);
               return;
             }

             buffer[result] = '\0';

             Json::Value responseJson;
             responseJson["path"] = decoded_path;
             responseJson["content"] = std::string(buffer.data());
             responseJson["size"] = static_cast<Json::UInt64>(st.st_size);

             auto resp =
                 drogon::HttpResponse::newHttpJsonResponse(responseJson);
             callback(resp);

           } catch (const std::exception &e) {
             Json::Value errorJson;
             errorJson["error"] = e.what();
             auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
             resp->setStatusCode(drogon::k500InternalServerError);
             callback(resp);
           }
         }},

        {"/semantic",
         [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::vector<std::string> &path) {
           try {
             auto &vfs =
                 vfs::instance::VFSInstance::getInstance().get_vector_fs();
             auto json = req->getJsonObject();

             if (!json || !json->isMember("query")) {
               Json::Value errorJson;
               errorJson["error"] = "Missing query parameter";
               auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
               resp->setStatusCode(drogon::k400BadRequest);
               callback(resp);
               return;
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

             auto resp =
                 drogon::HttpResponse::newHttpJsonResponse(responseJson);
             callback(resp);
           } catch (const std::exception &e) {
             Json::Value errorJson;
             errorJson["error"] = e.what();
             auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
             resp->setStatusCode(drogon::k500InternalServerError);
             callback(resp);
           }
         }},
        {"/rebuild",
         [](const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            const std::vector<std::string> &path) {

         }}};

} // namespace handler
} // namespace vfs::network

#endif // VECTORFS_NETWORK_HANDLERS_HPP