#ifndef VECTORFS_NETWORK_FILE_CONTROLLER_HPP
#define VECTORFS_NETWORK_FILE_CONTROLLER_HPP

#include "vectorfs.hpp"
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>

namespace vfs::network {
using namespace drogon;

class FileController : public HttpController<FileController> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(FileController::createFile, "/files/{path}", Post);
  METHOD_ADD(FileController::readFile, "/files/{path}", Get);
  METHOD_ADD(FileController::updateFile, "/files/{path}", Put);
  METHOD_ADD(FileController::deleteFile, "/files/{path}", Delete);
  METHOD_ADD(FileController::listFiles, "/files", Get);
  METHOD_ADD(FileController::createDirectory, "/directories/{path}", Post);
  METHOD_ADD(FileController::listDirectory, "/directories/{path}", Get);
  METHOD_LIST_END

  Task<HttpResponsePtr> createFile(const HttpRequestPtr req, std::string path);
  Task<HttpResponsePtr> readFile(const HttpRequestPtr req, std::string path);
  Task<HttpResponsePtr> updateFile(const HttpRequestPtr req, std::string path);
  Task<HttpResponsePtr> deleteFile(const HttpRequestPtr req, std::string path);
  Task<HttpResponsePtr> listFiles(const HttpRequestPtr req);
  Task<HttpResponsePtr> createDirectory(const HttpRequestPtr req,
                                        std::string path);
  Task<HttpResponsePtr> listDirectory(const HttpRequestPtr req,
                                      std::string path);

private:
  Json::Value fileInfoToJson(const std::string &path,
                             const fileinfo::FileInfo &info);
};

inline Task<HttpResponsePtr>
FileController::createFile(const HttpRequestPtr req, std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
  auto json = req->getJsonObject();

  if (!json || !json->isMember("content")) {
    Json::Value errorJson;
    errorJson["error"] = "Missing content";
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k400BadRequest);
    co_return resp;
  }

  try {
    path = drogon::utils::urlDecode(path);

    struct fuse_file_info fi {};
    auto result = vfs.create(path.c_str(), 0644, &fi);

    if (result != 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to create file";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    const std::string content = (*json)["content"].asString();
    result = vfs.write(path.c_str(), content.c_str(), content.size(), 0, &fi);

    if (result < 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to write content";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    struct stat st {};
    vfs.getattr(path.c_str(), &st, nullptr);

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["size"] = static_cast<Json::UInt64>(st.st_size);
    responseJson["created"] = true;

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    resp->setStatusCode(k201Created);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr> FileController::readFile(const HttpRequestPtr req,
                                                      std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    path = drogon::utils::urlDecode(path);

    struct stat st {};
    if (vfs.getattr(path.c_str(), &st, nullptr) != 0) {
      Json::Value errorJson;
      errorJson["error"] = "File not found";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k404NotFound);
      co_return resp;
    }

    std::vector<char> buffer(st.st_size + 1);
    auto result = vfs.read(path.c_str(), buffer.data(), st.st_size, 0, nullptr);

    if (result < 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to read file";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    buffer[result] = '\0';

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["content"] = std::string(buffer.data());
    responseJson["size"] = static_cast<Json::UInt64>(st.st_size);

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr>
FileController::updateFile(const HttpRequestPtr req, std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();
  auto json = req->getJsonObject();

  if (!json || !json->isMember("content")) {
    Json::Value errorJson;
    errorJson["error"] = "Missing content";
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k400BadRequest);
    co_return resp;
  }

  try {
    path = drogon::utils::urlDecode(path);

    struct stat st {};
    if (vfs.getattr(path.c_str(), &st, nullptr) != 0) {
      Json::Value errorJson;
      errorJson["error"] = "File not found";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k404NotFound);
      co_return resp;
    }

    struct fuse_file_info fi {};
    fi.flags = O_WRONLY | O_TRUNC;
    auto result = vfs.open(path.c_str(), &fi);

    if (result != 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to open file";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    const std::string content = (*json)["content"].asString();
    result = vfs.write(path.c_str(), content.c_str(), content.size(), 0, &fi);

    if (result < 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to write content";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    vfs.getattr(path.c_str(), &st, nullptr);

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["size"] = static_cast<Json::UInt64>(st.st_size);
    responseJson["updated"] = true;

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr>
FileController::deleteFile(const HttpRequestPtr req, std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    path = drogon::utils::urlDecode(path);

    struct stat st {};
    if (vfs.getattr(path.c_str(), &st, nullptr) != 0) {
      Json::Value errorJson;
      errorJson["error"] = "File not found";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k404NotFound);
      co_return resp;
    }

    auto result = vfs.unlink(path.c_str());

    if (result != 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to delete file";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["deleted"] = true;

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr>
FileController::listFiles(const HttpRequestPtr req) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    std::vector<std::string> files;
    // files = vfs.list_directory("/");

    Json::Value jsonFiles(Json::arrayValue);
    for (const auto &file : files) {
      struct stat st {};
      if (vfs.getattr(file.c_str(), &st, nullptr) == 0) {
        Json::Value fileInfo;
        fileInfo["path"] = file;
        fileInfo["size"] = static_cast<Json::UInt64>(st.st_size);
        fileInfo["is_directory"] = S_ISDIR(st.st_mode);
        jsonFiles.append(fileInfo);
      }
    }

    Json::Value responseJson;
    responseJson["files"] = jsonFiles;
    responseJson["count"] = static_cast<int>(files.size());

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr>
FileController::createDirectory(const HttpRequestPtr req, std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    path = drogon::utils::urlDecode(path);

    auto result = vfs.mkdir(path.c_str(), 0755);

    if (result != 0) {
      Json::Value errorJson;
      errorJson["error"] = "Failed to create directory";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k500InternalServerError);
      co_return resp;
    }

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["created"] = true;

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    resp->setStatusCode(k201Created);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Task<HttpResponsePtr>
FileController::listDirectory(const HttpRequestPtr req, std::string path) {
  auto &vfs = vfs::instance::VFSInstance::getInstance().get_vector_fs();

  try {
    path = drogon::utils::urlDecode(path);

    struct stat st {};
    if (vfs.getattr(path.c_str(), &st, nullptr) != 0 || !S_ISDIR(st.st_mode)) {
      Json::Value errorJson;
      errorJson["error"] = "Directory not found";
      auto resp = HttpResponse::newHttpJsonResponse(errorJson);
      resp->setStatusCode(k404NotFound);
      co_return resp;
    }

    std::vector<std::string> entries;
    // entries = vfs.list_directory(path);

    Json::Value jsonEntries(Json::arrayValue);
    for (const auto &entry : entries) {
      std::string fullPath = path + "/" + entry;
      struct stat entry_st {};
      if (vfs.getattr(fullPath.c_str(), &entry_st, nullptr) == 0) {
        Json::Value entryInfo;
        entryInfo["name"] = entry;
        entryInfo["path"] = fullPath;
        entryInfo["size"] = static_cast<Json::UInt64>(entry_st.st_size);
        entryInfo["is_directory"] = S_ISDIR(entry_st.st_mode);
        jsonEntries.append(entryInfo);
      }
    }

    Json::Value responseJson;
    responseJson["path"] = path;
    responseJson["entries"] = jsonEntries;
    responseJson["count"] = static_cast<int>(entries.size());

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    co_return resp;

  } catch (const std::exception &e) {
    Json::Value errorJson;
    errorJson["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(k500InternalServerError);
    co_return resp;
  }
}

inline Json::Value
FileController::fileInfoToJson(const std::string &path,
                               const fileinfo::FileInfo &info) {
  Json::Value json;
  json["path"] = path;
  json["size"] = static_cast<Json::UInt64>(info.size);
  json["mode"] = info.mode;
  json["uid"] = info.uid;
  json["gid"] = info.gid;
  json["access_time"] = static_cast<Json::UInt64>(info.access_time);
  json["modification_time"] = static_cast<Json::UInt64>(info.modification_time);
  json["embedding_updated"] = info.embedding_updated;
  json["embedding_size"] = static_cast<Json::UInt64>(info.embedding.size());
  return json;
}

} // namespace vfs::network
#endif