#ifndef OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP
#define OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <embedded/embedded_base.hpp>
#include <memory/container_builder.hpp>
#include <memory/pid_container.hpp>
#include <search/search.hpp>
#include <semantic/semantic_chunker.hpp>

#include "container_manager.hpp"
#include "container_states.hpp"
#include <infrastructure/result.hpp>

namespace owl {

template <typename EmbedderT = EmbedderManager<>,
          typename SearchT = chunkees::Search>
class OssecContainer
    : public ContainerBase<OssecContainer<EmbedderT, SearchT>> {
public:
  using Self = OssecContainer<EmbedderT, SearchT>;
  using Error = std::runtime_error;

  OssecContainer(std::shared_ptr<ossec::PidContainer> native,
                 std::string model_path)
      : native_(std::move(native)), embedder_manager_(std::move(model_path)),
        search_(std::make_unique<SearchT>(embedder_manager_)),
        fsm_(StateVariant{container::Unknown{}}, ContainerTransitionTable{}) {
    initializeSearch();
  }

  // ---------- CRTP: IdentifiableContainer ----------

  std::string getId() const { return native_->get_container().container_id; }

  std::string getOwner() const { return native_->get_container().owner_id; }

  std::string getNamespace() const {
    return native_->get_container().vectorfs_config.mount_namespace;
  }

  std::string getDataPath() const {
    return native_->get_container().data_path.string();
  }

  std::vector<std::string> getCommands() const {
    return native_->get_container().vectorfs_config.commands;
  }

  std::map<std::string, std::string> getLabels() const {
    return native_->get_container().labels;
  }

  // ---------- CRTP: FileSystemContainer ----------

  core::Result<std::vector<std::string>>
  listFiles(const std::string &virtual_path) const {
    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path.empty() || real_path == "/") {
      real_path = "";
    } else if (real_path.front() == '/') {
      real_path.erase(real_path.begin());
    }

    spdlog::info("📁 listFiles: FUSE='{}' -> REAL='{}'", virtual_path,
                 data_path.string());

    std::vector<std::string> files;

    if (!fs::exists(data_path) || !fs::is_directory(data_path)) {
      spdlog::warn("Directory not found: {}", data_path.string());
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(files));
    }

    // без try/catch: let it throw? — но по требованию
    // Result лучше поймаем и завернём:
    try {
      for (const auto &entry : fs::directory_iterator(data_path)) {
        if (entry.is_regular_file() || entry.is_directory()) {
          files.push_back(entry.path().filename().string());
        }
      }
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(files));
    } catch (const std::exception &e) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("listFiles error: ") + e.what()));
    }
  }

  core::Result<bool> fileExists(const std::string &virtual_path) const {
    if (virtual_path.empty() || virtual_path == "/") {
      return core::Result<bool, Error>::Ok(true);
    }

    auto data_path = native_->get_container().data_path;

    std::string real_path = virtual_path;
    if (!real_path.empty() && real_path.front() == '/') {
      real_path.erase(real_path.begin());
    }

    auto full_path = data_path / real_path;
    bool exists = std::filesystem::exists(full_path);

    spdlog::info("🔍 file_exists: FUSE='{}' -> REAL='{}' -> exists={}",
                 virtual_path, full_path.string(), exists);

    return core::Result<bool, Error>::Ok(exists);
  }

  core::Result<bool> isDirectory(const std::string &virtual_path) const {
    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path.empty() || real_path == "/") {
      real_path = "";
    } else if (real_path.front() == '/') {
      real_path.erase(real_path.begin());
    }

    auto full_path = data_path / real_path;

    try {
      bool is_dir = fs::exists(full_path) && fs::is_directory(full_path);
      return core::Result<bool, Error>::Ok(is_dir);
    } catch (const std::exception &e) {
      return core::Result<bool, Error>::Error(
          Error(std::string("isDirectory error: ") + e.what()));
    }
  }

  core::Result<std::string>
  getFileContent(const std::string &virtual_path) const {
    if (virtual_path.empty() || virtual_path == "/") {
      return core::Result<std::string, Error>::Ok(std::string{});
    }

    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;
    std::string real_path = virtual_path;
    if (!real_path.empty() && real_path.front() == '/') {
      real_path.erase(real_path.begin());
    }

    auto full_path = data_path / real_path;

    spdlog::info("📖 get_file_content: FUSE='{}' -> REAL='{}'", virtual_path,
                 full_path.string());

    try {
      if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        return core::Result<std::string, Error>::Error(
            Error("file not found: " + full_path.string()));
      }

      std::ifstream file(full_path);
      std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
      return core::Result<std::string, Error>::Ok(std::move(content));
    } catch (const std::exception &e) {
      return core::Result<std::string, Error>::Error(
          Error(std::string("getFileContent error: ") + e.what()));
    }
  }

  core::Result<void> addFile(const std::string &path,
                             const std::string &content) {
    auto data_path = native_->get_container().data_path;

    std::string normalized = path;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }

    auto full_path = data_path / normalized;

    try {
      std::filesystem::create_directories(full_path.parent_path());

      std::ofstream file(full_path);
      if (!file) {
        return core::Result<void, Error>::Error(
            Error("failed to open file for write: " + full_path.string()));
      }
      file << content;

      std::string search_path = "/" + normalized;

      auto r = search_->addFile(search_path, content);
      if (!r.is_ok()) {
        return core::Result<void, Error>::Error(
            Error("search::addFile failed: " + std::string(r.error().what())));
      }

      search_->updateSemanticRelationships();

      auto rebuild = search_->rebuildIndex();
      if (!rebuild.is_ok()) {
        spdlog::warn("Failed to rebuild index: {}", rebuild.error().what());
      }

      recordFileAccess(search_path, "write");
      return core::Result<void, Error>::Ok();
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("addFile error: ") + e.what()));
    }
  }

  core::Result<void> removeFile(const std::string &path) {
    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      bool removed = fs::remove(full_path);
      if (!removed) {
        return core::Result<void, Error>::Error(
            Error("file not removed: " + full_path.string()));
      }

      auto r = search_->removeFile(path);
      if (!r.is_ok()) {
        spdlog::warn("Failed to remove from index: {}", r.error().what());
      }

      search_->updateSemanticRelationships();
      return core::Result<void, Error>::Ok();
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("removeFile error: ") + e.what()));
    }
  }

  core::Result<std::vector<std::string>>
  searchFiles(const std::string &pattern) const {
    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;
    std::vector<std::string> results;

    try {
      for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
        if (!entry.is_regular_file())
          continue;
        std::string filename = entry.path().filename().string();
        if (filename.find(pattern) != std::string::npos) {
          results.push_back(fs::relative(entry.path(), data_path).string());
        }
      }
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(results));
    } catch (const std::exception &e) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("searchFiles error: ") + e.what()));
    }
  }

  core::Result<size_t> getSize() const {
    namespace fs = std::filesystem;

    auto data_path = native_->get_container().data_path;

    try {
      if (!fs::exists(data_path)) {
        return core::Result<size_t, Error>::Ok(0);
      }
      size_t total = 0;
      for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
        if (entry.is_regular_file()) {
          total += entry.file_size();
        }
      }
      return core::Result<size_t, Error>::Ok(total);
    } catch (const std::exception &e) {
      return core::Result<size_t, Error>::Error(
          Error(std::string("getSize error: ") + e.what()));
    }
  }

  // ---------- CRTP: ResourceManagedContainer ----------

  core::Result<void> setResourceLimit(const std::string &resource_name,
                                      const std::string &value) {
    if (!native_) {
      return core::Result<void, Error>::Error(
          Error("native container is null"));
    }

    try {
      auto &cont = native_->get_container();
      if (resource_name == "memory") {
        std::size_t mb = std::stoull(value);
        std::size_t bytes = mb * 1024 * 1024;
        cont.resources.memory_capacity = bytes;
        if (!cont.cgroup_path.empty()) {
          ossec::PidResources::set_memory_limit(cont.cgroup_path, bytes);
        }
        return core::Result<void, Error>::Ok();
      } else if (resource_name == "disk") {
        std::size_t mb = std::stoull(value);
        std::size_t bytes = mb * 1024 * 1024;
        cont.resources.storage_quota = bytes;
        return core::Result<void, Error>::Ok();
      } else if (resource_name == "pids") {
        std::size_t max_pids = std::stoull(value);
        cont.resources.max_open_files = max_pids;
        if (!cont.cgroup_path.empty()) {
          ossec::PidResources::set_pids_limit(cont.cgroup_path, max_pids);
        }
        return core::Result<void, Error>::Ok();
      } else if (resource_name == "apply") {
        spdlog::info("Applying resource changes for container {}", getId());
        return core::Result<void, Error>::Ok();
      }

      return core::Result<void, Error>::Error(
          Error("unknown resource_name: " + resource_name));
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("setResourceLimit error: ") + e.what()));
    }
  }

  core::Result<std::string> getCurrentResources() const {
    std::stringstream ss;
    if (!native_) {
      return core::Result<std::string, Error>::Error(
          Error("native container is null"));
    }

    const auto &cont = native_->get_container();

    ss << "=== Current Resource Limits ===\n\n";
    ss << "Memory: " << cont.resources.memory_capacity << " bytes ";
    ss << "(" << cont.resources.memory_capacity / (1024 * 1024) << " MB)\n";

    ss << "Disk: " << cont.resources.storage_quota << " bytes ";
    ss << "(" << cont.resources.storage_quota / (1024 * 1024) << " MB)\n";

    ss << "Max Processes/Files: " << cont.resources.max_open_files << "\n";

    ss << "\nChange with: echo 'VALUE' > /containers/" << getId()
       << "/.resources/RESOURCE_NAME\n";
    ss << "Apply changes: echo 'apply' > /containers/" << getId()
       << "/.resources/apply\n";

    return core::Result<std::string, Error>::Ok(ss.str());
  }

  // ---------- SearchableContainer ----------

  core::Result<std::vector<std::pair<std::string, float>>>
  semanticSearch(const std::string &query, int limit) {
    recordSearchQuery(query);

    auto r = search_->hybridSemanticSearch(query, limit);
    if (!r.is_ok()) {
      return core::Result<std::vector<std::pair<std::string, float>>, Error>::
          Error(
              Error(std::string("semanticSearch error: ") + r.error().what()));
    }

    const auto &res = r.value();
    std::vector<std::pair<std::string, float>> out;
    out.reserve(res.size());
    for (const auto &[file_path, score] : res) {
      out.emplace_back(file_path, score);
      recordFileAccess(file_path, "semantic_search");
    }
    return core::Result<std::vector<std::pair<std::string, float>>, Error>::Ok(
        std::move(out));
  }

  core::Result<std::vector<std::pair<std::string, float>>>
  enhancedSemanticSearch(const std::string &query, int limit) {
    recordSearchQuery(query);

    auto r = search_->enhancedSemanticSearch(query, limit);
    if (!r.is_ok()) {
      // fallback на semanticSearch
      return semanticSearch(query, limit);
    }

    std::vector<std::pair<std::string, float>> out;
    for (const auto &[file_path, score] : r.value()) {
      out.emplace_back(file_path, score);
      recordFileAccess(file_path, "enhanced_search");
    }
    return core::Result<std::vector<std::pair<std::string, float>>, Error>::Ok(
        std::move(out));
  }

  core::Result<std::vector<std::string>>
  getRecommendations(const std::string &current_file, int limit) {
    recordFileAccess(current_file, "recommendation_request");

    auto r = search_->getRecommendations(current_file);
    if (!r.is_ok()) {
      auto predictions = search_->predictNextFiles();
      if (predictions.is_ok()) {
        auto v = predictions.value();
        if ((int)v.size() > limit)
          v.resize(limit);
        return core::Result<std::vector<std::string>, Error>::Ok(std::move(v));
      }
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("getRecommendations failed: ") + r.error().what()));
    }

    auto recommendations = r.value();
    if ((int)recommendations.size() > limit) {
      recommendations.resize(limit);
    }
    return core::Result<std::vector<std::string>, Error>::Ok(
        std::move(recommendations));
  }

  core::Result<std::vector<std::string>> predictNextFiles(int /*limit*/) {
    auto r = search_->predictNextFiles();
    if (!r.is_ok()) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("predictNextFiles error: ") + r.error().what()));
    }
    return core::Result<std::vector<std::string>, Error>::Ok(r.value());
  }

  core::Result<std::vector<std::string>> getSemanticHubs(int count) {
    auto r = search_->getSemanticHubs(count);
    if (!r.is_ok()) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("getSemanticHubs error: ") + r.error().what()));
    }
    return core::Result<std::vector<std::string>, Error>::Ok(r.value());
  }

  core::Result<std::string> classifyFile(const std::string &file_path) {
    std::string category = search_->classifyFileCategory(file_path);
    return core::Result<std::string, Error>::Ok(std::move(category));
  }

  core::Result<void> updateAllEmbeddings() {
    auto files_res = listFiles("/");
    if (!files_res.is_ok()) {
      return core::Result<void, Error>::Error(files_res.error());
    }

    const auto &files = files_res.value();
    for (const auto &file : files) {
      auto file_path = "/" + file;
      auto r = search_->updateEmbedding(file_path);
      if (!r.is_ok()) {
        spdlog::warn("Failed to update embedding for {}: {}", file_path,
                     r.error().what());
      }
    }

    auto rebuild = search_->rebuildIndex();
    if (!rebuild.is_ok()) {
      spdlog::warn("Failed to rebuild index: {}", rebuild.error().what());
    }

    search_->updateSemanticRelationships();
    return core::Result<void, Error>::Ok();
  }

  core::Result<std::string> getSearchInfo() const {
    auto file_count = search_->getIndexedFilesCount();
    auto recent_queries = search_->getRecentQueriesCount();

    std::stringstream ss;
    ss << "Search Info for Container " << getId() << ":\n";
    ss << "  Indexed Files: " << (file_count.is_ok() ? file_count.value() : 0)
       << "\n";
    ss << "  Recent Queries: "
       << (recent_queries.is_ok() ? recent_queries.value() : 0) << "\n";
    ss << "  Embedder: " << search_->getEmbedderInfo() << "\n";

    return core::Result<std::string, Error>::Ok(ss.str());
  }

  core::Result<void> recordSearchQuery(const std::string &query) {
    auto r = search_->getRecentQueries();
    if (!r.is_ok()) {
      return core::Result<void, Error>::Error(
          Error(std::string("recordSearchQuery error: ") + r.error().what()));
    }
    spdlog::trace("Recorded search query: {}", query);
    return core::Result<void, Error>::Ok();
  }

  // ---------- StatefulContainer ----------

  std::string getStatus() const {
    auto native_status = native_ ? native_->is_running() : false;
    if (!native_)
      return "invalid";
    if (native_status)
      return "running";
    if (native_->is_owned())
      return "stopped";
    return "unknown";
  }

  bool isAvailable() const { return native_ && native_->is_running(); }

  // ---------- FSM helper ----------

  core::Result<void> ensureRunning() {
    if (!native_) {
      return core::Result<void, Error>::Error(Error("native is null"));
    }
    if (!native_->is_running()) {
      auto r = native_->start();
      if (!r.is_ok()) {
        return core::Result<void, Error>::Error(
            Error(std::string("start failed: ") + r.error().what()));
      }

      return core::Result<void, Error>::Ok();
    }
    return core::Result<void, Error>::Ok();
  }

  std::shared_ptr<ossec::PidContainer> getNative() const { return native_; }

  EmbedderT &embedder() { return embedder_manager_; }
  const EmbedderT &embedder() const { return embedder_manager_; }

  SearchT &search() { return *search_; }
  const SearchT &search() const { return *search_; }

private:
  void initializeSearch() {
    auto files_res = listFiles("/");
    if (!files_res.is_ok()) {
      spdlog::warn("initializeSearch: {}", files_res.error().what());
      return;
    }

    const auto &files = files_res.value();
    for (const auto &file : files) {
      std::string path = "/" + file;
      auto content_res = getFileContent(path);
      if (!content_res.is_ok()) {
        spdlog::warn("Failed to get content for {}: {}", path,
                     content_res.error().what());
        continue;
      }

      auto r = search_->addFile(path, content_res.value());
      if (r.is_ok()) {
        recordFileAccess(path, "read");
      } else {
        spdlog::warn("Failed to index file {}: {}", path, r.error().what());
      }
    }

    search_->updateSemanticRelationships();
    auto rebuild = search_->rebuildIndex();
    if (!rebuild.is_ok()) {
      spdlog::warn("Failed to rebuild index after init: {}",
                   rebuild.error().what());
    }
  }

  void recordFileAccess(const std::string &file_path,
                        const std::string &operation) {
    auto r = search_->recordFileAccess(file_path, operation);
    if (!r.is_ok()) {
      spdlog::debug("Failed to record file access: {} - {}", file_path,
                    r.error().what());
    }
  }

private:
  std::shared_ptr<ossec::PidContainer> native_;
  EmbedderT embedder_manager_;
  std::unique_ptr<SearchT> search_;
  ContainerStateMachine fsm_;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP