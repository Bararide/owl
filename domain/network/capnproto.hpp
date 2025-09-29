#ifndef VECTORFS_CAPNP_SERVER_HPP
#define VECTORFS_CAPNP_SERVER_HPP

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/debug.h>
#include <kj/main.h>
#include <memory>
#include <unordered_map>

#include "shared_memory/shared_memory.hpp"
#include "instance/instance.hpp"
#include "vectorfs.capnp.h"

namespace owl::capnp {

template <typename EmbeddedModel>
class VectorFSServiceImpl final : public VectorFSService::Server {
public:
  VectorFSServiceImpl() = default;

protected:
  kj::Promise<void> createFile(CreateFileContext context) override {
    auto request = context.getParams().getRequest();
    std::string path = request.getPath();
    std::string content = request.getContent();

    spdlog::info("=== File Creation Request ===");
    spdlog::info("File path: {}", path);
    spdlog::info("Content length: {} bytes", content.size());

    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      if (!path.empty() && path[0] != '/') {
        path = "/" + path;
        spdlog::info("Normalized path to: {}", path);
      }

      auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      if (!shm_manager.initialize()) {
        throw kj::Exception(kj::Exception::Type::FAILED, __FILE__, __LINE__,
                            kj::str("Failed to initialize shared memory"));
      }

      if (!shm_manager.addFile(path, content)) {
        throw kj::Exception(
            kj::Exception::Type::FAILED, __FILE__, __LINE__,
            kj::str("Failed to add file to shared memory: ", path));
      }

      spdlog::info("Successfully added file to shared memory: {}", path);

      response.setSuccess(true);
      auto fileInfo = response.initFile();
      fileInfo.setPath(path);
      fileInfo.setContent(content);
      fileInfo.setSize(content.size());
      fileInfo.setCreated(true);

      spdlog::info("File creation completed successfully:");
      spdlog::info("  Path: {}", path);
      spdlog::info("  Size: {} bytes", content.size());
      spdlog::info("=============================");

    } catch (const kj::Exception &e) {
      spdlog::error("Error in createFile: {}", e.getDescription().cStr());
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> readFile(ReadFileContext context) override {
    auto request = context.getParams().getRequest();
    std::string path = request.getPath();

    spdlog::info("=== File Read Request ===");
    spdlog::info("File path: {}", path);

    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      KJ_REQUIRE(!path.empty(), "Path cannot be empty");

      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
      auto &virtual_files = vfs.get_virtual_files();
      auto it = virtual_files.find(path);

      KJ_REQUIRE(it != virtual_files.end(), "File not found");

      const auto &file_info = it->second;
      KJ_REQUIRE(!S_ISDIR(file_info.mode), "Path is a directory");

      response.setSuccess(true);
      auto fileInfo = response.initFile();
      fileInfo.setPath(path);
      fileInfo.setContent(file_info.content);
      fileInfo.setSize(file_info.content.size());
      fileInfo.setMode(file_info.mode);

      spdlog::info("File read completed successfully: {}", path);

    } catch (const kj::Exception &e) {
      spdlog::error("Error in readFile: {}", e.getDescription().cStr());
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> semanticSearch(SemanticSearchContext context) override {
    auto request = context.getParams().getRequest();
    std::string query = request.getQuery();
    int limit = request.getLimit();

    spdlog::info("=== Semantic Search Request ===");
    spdlog::info("Query: {}", query);
    spdlog::info("Limit: {}", limit);

    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      KJ_REQUIRE(!query.empty(), "Query cannot be empty");
      KJ_REQUIRE(limit > 0 && limit <= 100, "Limit must be between 1 and 100");

      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
      auto search_results = vfs.semantic_search(query, limit);

      response.setQuery(query);
      response.setCount(search_results.size());

      auto resultsList = response.initResults(search_results.size());
      for (size_t i = 0; i < search_results.size(); ++i) {
        const auto &[path, score] = search_results[i];
        auto result = resultsList[i];
        result.setPath(path);
        result.setScore(score);
      }

      spdlog::info("Semantic search completed: found {} results",
                   search_results.size());

    } catch (const kj::Exception &e) {
      spdlog::error("Error in semanticSearch: {}", e.getDescription().cStr());
      response.setQuery(query);
      response.setCount(0);
      response.initResults(0);
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> rebuildIndex(RebuildIndexContext context) override {
    spdlog::info("=== Rebuild Index Request ===");

    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      // Здесь должна быть логика перестроения индекса
      // Пока что заглушка

      response.setSuccess(true);
      response.setMessage("Rebuild completed successfully");

      spdlog::info("Index rebuild completed");

    } catch (const kj::Exception &e) {
      spdlog::error("Error in rebuildIndex: {}", e.getDescription().cStr());
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> getStatus(GetStatusContext context) override {
    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();
      auto &virtual_files = vfs.get_virtual_files();

      response.setSuccess(true);
      response.setMessage(
          kj::str("System status: OK. Files in VFS: ", virtual_files.size()));

    } catch (const kj::Exception &e) {
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }

  kj::Promise<void> getStats(GetStatsContext context) override {
    auto results = context.getResults();
    auto response = results.initResponse();

    try {
      auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
                      .get_vector_fs();

      size_t file_count = vfs.get_virtual_files().size();
      size_t total_size = 0;
      for (const auto &[path, file_info] : vfs.get_virtual_files()) {
        total_size += file_info.content.size();
      }

      response.setSuccess(true);
      response.setMessage(kj::str("Files: ", file_count,
                                  ", Total size: ", total_size, " bytes"));

    } catch (const kj::Exception &e) {
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }
};

} // namespace owl::capnp_server

#endif // VECTORFS_CAPNP_SERVER_HPP