#ifndef VECTORFS_NETWORK_SEARCH_CONTROLLER_HPP
#define VECTORFS_NETWORK_SEARCH_CONTROLLER_HPP

#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include "vectorfs.hpp"

namespace vfs::network {
using namespace drogon;

class SearchController : public HttpController<SearchController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(SearchController::semanticSearch, "/semantic", Post);
    METHOD_ADD(SearchController::rebuildIndex, "/index/rebuild", Post);
    METHOD_ADD(SearchController::getIndexStatus, "/index/status", Get);
    METHOD_LIST_END

    Task<HttpResponsePtr> semanticSearch(const HttpRequestPtr req);
    Task<HttpResponsePtr> rebuildIndex(const HttpRequestPtr req);
    Task<HttpResponsePtr> getIndexStatus(const HttpRequestPtr req);
};

inline Task<HttpResponsePtr> SearchController::semanticSearch(const HttpRequestPtr req) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
  auto json = req->getJsonObject();

  if (!json || !json->isMember("query")) {
    auto resp = HttpResponse::newHttpJsonResponse(
        Json::Value{{"error", "Missing query parameter"}});
    resp->setStatusCode(k400BadRequest);
    co_return resp;
  }

  try {
    const std::string query = (*json)["query"].asString();
    const int limit = json->get("limit", 10).asInt();

    auto results = vfs.semantic_search(query, limit);

    Json::Value jsonResults(Json::arrayValue);
    for (const auto &[path, score] : results) {
      Json::Value result;
      result["path"] = path;
      result["score"] = score;

      struct stat st {};
      if (vfs.getattr(path.c_str(), &st, nullptr) == 0) {
        result["size"] = static_cast<Json::UInt64>(st.st_size);
        result["modified"] = static_cast<Json::UInt64>(st.st_mtime);
      }

      jsonResults.append(result);
    }

    Json::Value response;
    response["query"] = query;
    response["results"] = jsonResults;
    response["count"] = static_cast<int>(results.size());

    auto resp = HttpResponse::newHttpJsonResponse(response);
    co_return resp;

  } catch (const std::exception &e) {
    auto resp =
        HttpResponse::newHttpJsonResponse(Json::Value{{"error", e.what()}});
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr> SearchController::rebuildIndex(const HttpRequestPtr req) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    auto start = std::chrono::high_resolution_clock::now();
    vfs.rebuild_index();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    Json::Value response;
    response["status"] = "success";
    response["duration_ms"] = duration.count();
    response["message"] = "Index rebuilt successfully";

    auto resp = HttpResponse::newHttpJsonResponse(response);
    co_return resp;

  } catch (const std::exception &e) {
    auto resp =
        HttpResponse::newHttpJsonResponse(Json::Value{{"error", e.what()}});
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr> SearchController::getIndexStatus(const HttpRequestPtr req) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    Json::Value response;
    response["status"] = "active";
    response["indexed_files"] = 0;
    response["index_size"] = 0;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    co_return resp;

  } catch (const std::exception &e) {
    auto resp =
        HttpResponse::newHttpJsonResponse(Json::Value{{"error", e.what()}});
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

} // namespace vfs::network
#endif