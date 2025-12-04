#ifndef VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP
#define VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP

#include "knowledge_container.hpp"
#include "search.hpp"
#include <filesystem>
#include <fstream>
#include <memory/pid_container.hpp>
#include <set>
#include <spdlog/spdlog.h>

namespace owl::vectorfs {

class OssecContainerAdapter final
    : public KnowledgeContainer<OssecContainerAdapter> {
public:
  OssecContainerAdapter(std::shared_ptr<ossec::PidContainer> container,
                        EmbedderManager<> &embedder_manager)
      : container_(std::move(container)),
        search_(std::make_unique<chunkees::Search>(embedder_manager)) {
    initializeSearch();
  }

  std::string getId() const noexcept {
    return container_->get_container().container_id;
  }

  std::string getOwner() const noexcept {
    return container_->get_container().owner_id;
  }

  std::string getNamespace() const noexcept {
    return container_->get_container().vectorfs_config.mount_namespace;
  }

  std::string getDataPath() const noexcept {
    return container_->get_container().data_path.string();
  }

  std::vector<std::string> getCommands() const noexcept {
    return container_->get_container().vectorfs_config.commands;
  }

  std::map<std::string, std::string> getLabels() const noexcept {
    return container_->get_container().labels;
  }

  std::vector<std::string>
  listFiles(const std::string &virtual_path) const {
    auto data_path = container_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path.empty() || real_path == "/") {
      real_path = "";
    } else if (real_path[0] == '/') {
      real_path = real_path.substr(1);
    }

    std::vector<std::string> files;

    spdlog::info("📁 list_files: FUSE='{}' -> REAL='{}'", virtual_path,
                 data_path.string());

    try {
      if (std::filesystem::exists(data_path) &&
          std::filesystem::is_directory(data_path)) {
        for (const auto &entry :
             std::filesystem::directory_iterator(data_path)) {
          if (entry.is_regular_file() || entry.is_directory()) {
            std::string filename = entry.path().filename().string();
            files.push_back(filename);
            spdlog::info("📄 Found: {}", filename);
          }
        }
      } else {
        spdlog::warn("Directory not found: {}", data_path.string());
      }
    } catch (const std::exception &e) {
      spdlog::error("Error listing files: {}", e.what());
    }

    return files;
  }

  bool fileExists(const std::string &virtual_path) const {
    if (virtual_path.empty() || virtual_path == "/") {
      return true;
    }

    auto data_path = container_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path[0] == '/') {
      real_path = real_path.substr(1);
    }

    auto full_path = data_path / real_path;
    bool exists = std::filesystem::exists(full_path);

    spdlog::info("🔍 file_exists: FUSE='{}' -> REAL='{}' -> exists={}",
                 virtual_path, full_path.string(), exists);

    return exists;
  }

  std::string getFileContent(const std::string &virtual_path) const {
    if (virtual_path.empty() || virtual_path == "/") {
      return "";
    }

    auto data_path = container_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path[0] == '/') {
      real_path = real_path.substr(1);
    }

    auto full_path = data_path / real_path;

    spdlog::info("📖 getFileContent: FUSE='{}' -> REAL='{}'", virtual_path,
                 full_path.string());

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        spdlog::info("✅ Successfully read file, size: {} bytes",
                     content.size());
        return content;
      } else {
        spdlog::warn("File not found: {}", full_path.string());
      }
    } catch (const std::exception &e) {
      spdlog::error("Error reading file: {}", e.what());
    }

    return "";
  }

  bool isDirectory(const std::string &virtual_path) const {
    auto data_path = container_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path.empty() || real_path == "/") {
      real_path = "";
    } else if (real_path[0] == '/') {
      real_path = real_path.substr(1);
    }

    auto full_path = data_path / real_path;

    try {
      bool is_dir = std::filesystem::exists(full_path) &&
                    std::filesystem::is_directory(full_path);
      spdlog::info("is_directory: virtual='{}', real='{}', is_dir={}",
                   virtual_path, real_path, is_dir);
      return is_dir;
    } catch (const std::exception &e) {
      spdlog::error("Error checking directory: {}", e.what());
      return false;
    }
  }
  bool addFile(const std::string &path, const std::string &content) {
    auto data_path = container_->get_container().data_path;

    std::string normalized_path = path;
    if (!normalized_path.empty() && normalized_path[0] == '/') {
      normalized_path = normalized_path.substr(1);
    }

    auto full_path = data_path / normalized_path;

    spdlog::info("📝 ADD_FILE: path='{}', normalized='{}', full='{}'", path,
                 normalized_path, full_path.string());

    try {
      std::filesystem::create_directories(full_path.parent_path());

      std::ofstream file(full_path);
      if (!file) {
        spdlog::error("❌ Failed to open file for writing: {}",
                      full_path.string());
        return false;
      }

      file << content;
      file.close();

      spdlog::info("✅ File written successfully: {}", full_path.string());

      std::string search_path = "/" + normalized_path;

      spdlog::info("🔍 Adding file to container search index: {}", search_path);

      auto result = search_->addFileImpl(search_path, content);
      if (!result.is_ok()) {
        spdlog::error("❌ Failed to add file to search index: {} - {}",
                      search_path, result.error().what());
        return false;
      }

      spdlog::info("✅ File added to search index: {}", search_path);

      spdlog::info("🔄 Updating semantic relationships...");
      search_->updateSemanticRelationships();

      auto rebuild_result = search_->rebuildIndexImpl();
      if (!rebuild_result.is_ok()) {
        spdlog::warn("Failed to rebuild container index: {}",
                     rebuild_result.error().what());
      }

      recordFileAccess(search_path, "write");

      spdlog::info(
          "🎉 File {} successfully added to container and search index",
          search_path);
      return true;

    } catch (const std::exception &e) {
      spdlog::error("❌ Exception while adding file {}: {}", path, e.what());
      return false;
    }
  }

  bool removeFile(const std::string &path) {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      bool removed = std::filesystem::remove(full_path);

      if (removed) {
        auto result = search_->removeFileImpl(path);
        if (!result.is_ok()) {
          spdlog::warn("Failed to remove file from search index: {} - {}", path,
                       result.error().what());
        }

        search_->updateSemanticRelationships();

        spdlog::info("File removed successfully: {}", path);
        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to remove file {}: {}", path, e.what());
    }

    return false;
  }

  void initializeSearch() {
    auto files = listFiles("/");
    size_t indexed_count = 0;

    for (const auto &file : files) {
      std::string file_path = "/" + file;
      std::string content = getFileContent(file_path);

      if (!content.empty()) {
        auto result = search_->addFileImpl(file_path, content);
        if (result.is_ok()) {
          indexed_count++;
          recordFileAccess(file_path, "read");
        } else {
          spdlog::warn("Failed to index file during initialization: {} - {}",
                       file_path, result.error().what());
        }
      }
    }

    search_->updateSemanticRelationships();

    auto rebuild_result = search_->rebuildIndexImpl();
    if (!rebuild_result.is_ok()) {
      spdlog::warn("Failed to rebuild index after initialization: {}",
                   rebuild_result.error().what());
    }
  }

  void initialize_markov_recommend_chain() {
    auto semantic_graph = search_->getSemanticGraph();
    auto hmm_model = search_->getHiddenModel();

    if (!semantic_graph || !hmm_model) {
      spdlog::error("Markov models not available for initialization");
      return;
    }

    semantic_graph->add_edge("code", "document", 0.7);
    semantic_graph->add_edge("code", "config", 0.5);
    semantic_graph->add_edge("code", "test", 0.8);
    semantic_graph->add_edge("document", "code", 0.6);
    semantic_graph->add_edge("config", "code", 0.9);
    semantic_graph->add_edge("test", "code", 0.85);
    semantic_graph->add_edge("document", "config", 0.3);
    semantic_graph->add_edge("config", "document", 0.4);

    hmm_model->add_state("code");
    hmm_model->add_state("document");
    hmm_model->add_state("config");
    hmm_model->add_state("test");
    hmm_model->add_state("misc");

    std::vector<std::vector<std::string>> training_sequences = {
        {"code", "document", "code", "test"},
        {"document", "code", "config", "code"},
        {"config", "code", "test", "code"},
        {"test", "code", "document", "config"},
        {"code", "test", "code", "misc"}};

    for (const auto &sequence : training_sequences) {
      hmm_model->add_sequence(sequence);
    }

    hmm_model->train();

    auto hubs = semantic_graph->random_walk_ranking(1000);

    if (!hubs.empty()) {
      spdlog::info("Top semantic hub: {} (score: {:.3f})", hubs[0].first,
                   hubs[0].second);
    }
  }

  std::vector<std::pair<std::string, float>>
  semanticSearch(const std::string &query, int limit = 10) {
    recordFileAccess(query);

    auto result = search_->semanticSearchImpl(query, limit);

    if (result.empty()) {
      spdlog::debug("No semantic search results for query: '{}'", query);
      return {};
    }

    std::vector<std::pair<std::string, float>> file_paths;
    for (const auto &[file_path, score] : result) {
      file_paths.push_back({file_path, score});

      recordFileAccess(file_path, "semantic_search");
    }

    return file_paths;
  }

  std::vector<std::string>
  searchFiles(const std::string &pattern) const {
    std::vector<std::string> results;
    auto data_path = container_->get_container().data_path;

    try {
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(data_path)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();
          if (filename.find(pattern) != std::string::npos) {
            std::string relative_path =
                std::filesystem::relative(entry.path(), data_path).string();
            results.push_back(relative_path);
          }
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("File search error: {}", e.what());
    }

    return results;
  }

  std::vector<std::pair<std::string, float>>
  enhancedSemanticSearch(const std::string &query, int limit) {
    spdlog::debug("Enhanced semantic search in container {}: '{}'", getId(),
                  query);

    recordFileAccess(query);

    auto result = search_->enhancedSemanticSearchImpl(query, limit);

    if (!result.is_ok()) {
      return semanticSearch(query, limit);
    }

    std::vector<std::pair<std::string, float>> file_paths;
    for (const auto &[file_path, score] : result.value()) {
      file_paths.push_back({file_path, score});
      recordFileAccess(file_path, "enhanced_search");
    }

    return file_paths;
  }

  std::vector<std::string> getRecommendations(const std::string &current_file,
                                               int limit) {
    recordFileAccess(current_file, "recommendation_request");

    auto result = search_->getRecommendationsImpl(current_file);

    if (!result.is_ok()) {
      auto predictions = search_->predictNextFilesImpl();
      if (predictions.is_ok()) {
        return predictions.value();
      }

      return {};
    }

    auto recommendations = result.value();

    if (recommendations.size() > limit) {
      recommendations.resize(limit);
    }

    return recommendations;
  }

  std::vector<std::string> predictNextFiles(int limit) {
    auto result = search_->predictNextFilesImpl();

    if (!result.is_ok()) {
      return {};
    }

    auto predictions = result.value();
    return predictions;
  }

  std::vector<std::string> getSemanticHubs(int count = 5) {
    auto result = search_->getSemanticHubsImpl(count);

    if (!result.is_ok()) {
      return {};
    }

    auto hubs = result.value();
    return hubs;
  }

  std::string classifyFile(const std::string &file_path) {
    std::string category = search_->classifyFileCategoryImpl(file_path);
    return category;
  }

  void recordFileAccess(const std::string &file_path,
                          const std::string &operation = "read") {
    auto result = search_->recordFileAccessImpl(file_path, operation);
    if (!result.is_ok()) {
      spdlog::debug("Failed to record file access: {} - {}", file_path,
                    result.error().what());
    } else {
      spdlog::trace("Recorded {} access to: {}", operation, file_path);
    }
  }

  void recordSearchQuery(const std::string &query) {
    auto recent_queries = search_->getRecentQueriesImpl();
    if (recent_queries.is_ok()) {
      spdlog::trace("Recorded search query: {}", query);
    }
  }

  bool ensure_running() {
    if (!container_->is_running()) {
      auto result = container_->start();
      return result.is_ok();
    }
    return true;
  }

  bool updateAllEmbeddings() {
    auto files = listFiles(container_->get_container().data_path.string());
    size_t updated_count = 0;

    for (const auto &file : files) {
      std::string file_path = "/" + file;

      auto result = search_->updateEmbedding(file_path);
      if (result.is_ok()) {
        updated_count++;
      } else {
        spdlog::warn("Failed to update embedding for {}: {}", file_path,
                     result.error().what());
      }
    }

    auto rebuild_result = search_->rebuildIndexImpl();
    if (!rebuild_result.is_ok()) {
      spdlog::warn("Failed to rebuild index after embedding update: {}",
                   rebuild_result.error().what());
    }

    search_->updateSemanticRelationships();
    return updated_count > 0;
  }
  
  bool isAvailable() const {
    return container_ && container_->is_running();
  }

  size_t getSize() const {
    try {
      auto data_path = container_->get_container().data_path;
      if (std::filesystem::exists(data_path)) {
        size_t total_size = 0;
        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(data_path)) {
          if (entry.is_regular_file()) {
            total_size += entry.file_size();
          }
        }
        return total_size;
      }
    } catch (const std::exception &e) {
      spdlog::debug("Error calculating container size: {}", e.what());
    }
    return 0;
  }

  std::string getStatus() const {
    if (!container_)
      return "invalid";
    if (container_->is_running())
      return "running";
    else if (container_->is_owned())
      return "stopped";
    else
      return "unknown";
  }

  std::shared_ptr<ossec::PidContainer> get_native_container() const {
    return container_;
  }

private:
  std::shared_ptr<ossec::PidContainer> container_;
  std::unique_ptr<chunkees::Search> search_;
};

} // namespace owl::vectorfs

#endif