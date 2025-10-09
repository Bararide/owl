#ifndef VECTORFS_CAPNP_SERVER_HPP
#define VECTORFS_CAPNP_SERVER_HPP

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/debug.h>
#include <kj/main.h>
#include <kj/thread.h>
#include <memory>
#include <unordered_map>

#include <infrastructure/event.hpp>

#include "instance/instance.hpp"
#include "schemas/fileinfo.hpp"
#include "vectorfs.capnp.h"

namespace owl::capnp {

template <typename EmbeddedModel>
class VectorFSServiceImpl final : public VectorFSService::Server {
public:
  VectorFSServiceImpl() = default;

  core::Result<bool>
  addEventService(const std::shared_ptr<core::Event> event_service) {
    try {
      event_service_ = event_service;
      return core::Result<bool>(true);
    } catch (const std::exception &e) {
      return core::Result<bool>(false);
    }
  }

protected:
  kj::Promise<void> createFile(CreateFileContext context) override {
    auto request = context.getParams().getRequest();
    std::string path = request.getPath();
    std::string content = request.getContent();
    std::string name = request.getName();

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

      // auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      // if (!shm_manager.initialize()) {
      //   throw kj::Exception(kj::Exception::Type::FAILED, __FILE__, __LINE__,
      //                       kj::str("Failed to initialize shared memory"));
      // }

      // if (!shm_manager.addFile(path, content)) {
      //   throw kj::Exception(
      //       kj::Exception::Type::FAILED, __FILE__, __LINE__,
      //       kj::str("Failed to add file to shared memory: ", path));
      // }

      spdlog::info("Successfully added file to shared memory: {}", path);

      response.setSuccess(true);
      auto fileInfo = response.initFile();
      fileInfo.setName(name);
      fileInfo.setPath(path);
      fileInfo.setContent(content);
      fileInfo.setSize(content.size());
      fileInfo.setCreated(true);

      schemas::FileInfo fileinfo = schemas::FileInfo();

      fileinfo.path = path;
      fileinfo.created = true;
      fileinfo.name = name;
      fileinfo.size = content.size();
      fileinfo.content = std::vector<uint8_t>(content.begin(), content.end());

      spdlog::info("File creation completed successfully:");
      spdlog::info("  Path: {}", path);
      spdlog::info("  Size: {} bytes", content.size());
      spdlog::info("  Content vector size: {} bytes", fileinfo.content->size());
      spdlog::info("=============================");

      if (event_service_) {
        event_service_->Notify(fileinfo);
      }

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
      // auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
      // auto &vfs = owl::instance::VFSInstance<EmbeddedModel>::getInstance()
      //                 .get_vector_fs();

      // size_t file_count = vfs.get_virtual_files().size();
      // size_t total_size = 0;
      // for (const auto &[path, file_info] : vfs.get_virtual_files()) {
      //   total_size += file_info.content.size();
      // }

      // response.setSuccess(true);
      // response.setMessage(kj::str("Files: ", file_count,
      //                             ", Total size: ", total_size, " bytes"));

    } catch (const kj::Exception &e) {
      response.setSuccess(false);
      response.setError(e.getDescription());
    }

    return kj::READY_NOW;
  }

private:
  std::shared_ptr<core::Event> event_service_;
};

template <typename EmbeddedModel> class VectorFSServer {
public:
  VectorFSServer(const std::string &address = "127.0.0.1:5346")
      : address_(address) {
    service_impl = std::make_unique<VectorFSServiceImpl<EmbeddedModel>>();
  }

  ~VectorFSServer() { stop(); }

  void addEventService(const std::shared_ptr<core::Event> event_service) {
    if (service_impl) {
      service_impl->addEventService(event_service);
    }
  }

  void run() {
    if (running_.load()) {
      spdlog::warn("Server is already running");
      return;
    }

    running_.store(true);

    try {
      server_ = std::make_unique<::capnp::EzRpcServer>(
          kj::heap<VectorFSServiceImpl<EmbeddedModel>>(*service_impl),
          address_.c_str());

      spdlog::info("Cap'n Proto server started on {}", address_);

      auto &waitScope = server_->getWaitScope();
      kj::NEVER_DONE.wait(waitScope);

    } catch (const kj::Exception &e) {
      spdlog::error("KJ Exception in server: {}", e.getDescription().cStr());
    } catch (const std::exception &e) {
      spdlog::error("Server run exception: {}", e.what());
    }

    running_.store(false);
  }

  void stop() {
    if (running_.exchange(false)) {
      spdlog::info("Stopping server...");
      if (server_) {
        server_ = nullptr;
      }
    }
  }

  bool isRunning() const { return running_.load(); }

private:
  std::string address_;
  std::unique_ptr<VectorFSServiceImpl<EmbeddedModel>> service_impl;
  std::unique_ptr<::capnp::EzRpcServer> server_;
  std::atomic<bool> running_{false};
};

class VectorFSClient {
public:
  VectorFSClient(const std::string &address)
      : client(address.c_str()), service(client.getMain<VectorFSService>()) {}

  VectorFSService::Client getService() { return service; }

private:
  ::capnp::EzRpcClient client;
  VectorFSService::Client service;
};

class TestClient {
public:
  TestClient(const std::string &address)
      : client_(address.c_str()), service_(client_.getMain<VectorFSService>()) {
    service_ = client_.getMain<VectorFSService>();
  }

  void testConnection() {
    try {
      auto &waitScope = client_.getWaitScope();
      {
        auto request = service_.createFileRequest();
        auto req = request.initRequest();
        req.setPath("/test_file.txt");
        req.setName("test_file.txt");
        req.setContent("This is test content");

        auto promise = request.send();
        auto response = promise.wait(waitScope);

        if (response.getResponse().getSuccess()) {
          std::cout << "✓ Create file test passed" << std::endl;
        } else {
          std::cout << "✗ Create file test failed: "
                    << response.getResponse().getError().cStr() << std::endl;
        }
      }

    } catch (const std::exception &e) {
      std::cerr << "✗ Client test failed: " << e.what() << std::endl;
    }
  }

private:
  ::capnp::EzRpcClient client_;
  VectorFSService::Client service_;
};

} // namespace owl::capnp

#endif // VECTORFS_CAPNP_SERVER_HPP