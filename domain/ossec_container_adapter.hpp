#ifndef VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP
#define VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP

#include "knowledge_container.hpp"
#include "search.hpp"
#include <filesystem>
#include <fstream>
#include <memory/pid_container.hpp>
#include <set>
#include <spdlog/spdlog.h>

namespace owl {

class OssecContainerAdapter : public IKnowledgeContainer {
public:
  OssecContainerAdapter(std::shared_ptr<ossec::PidContainer> container,
                        EmbedderManager<> &embedder_manager)
      : container_(std::move(container)),
        search_(std::make_unique<chunkees::Search>(embedder_manager)) {
    initialize_search();
  }

  std::string get_id() const override {
    return container_->get_container().container_id;
  }

  std::string get_owner() const override {
    return container_->get_container().owner_id;
  }

  std::string get_namespace() const override {
    return container_->get_container().vectorfs_config.mount_namespace;
  }

  std::string get_data_path() const override {
    return container_->get_container().data_path.string();
  }

  std::vector<std::string> get_commands() const override {
    return container_->get_container().vectorfs_config.commands;
  }

  std::map<std::string, std::string> get_labels() const override {
    return container_->get_container().labels;
  }

  std::vector<std::string>
  list_files(const std::string &virtual_path) const override {
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

  bool set_resource_limit(const std::string &resource_name,
                          const std::string &value) override {
    if (!container_) {
      return false;
    }

    try {
      if (resource_name == "memory") {
        size_t mb = std::stoull(value);
        size_t bytes = mb * 1024 * 1024;

        container_->get_container().resources.memory_capacity = bytes;

        if (!container_->get_container().cgroup_path.empty()) {
          ossec::PidResources::set_memory_limit(
              container_->get_container().cgroup_path, bytes);
        }

        spdlog::info("Container {} memory limit set to {} MB", get_id(), mb);
        return true;

      } else if (resource_name == "disk") {
        size_t mb = std::stoull(value);
        size_t bytes = mb * 1024 * 1024;
        container_->get_container().resources.storage_quota = bytes;
        spdlog::info("Container {} disk quota set to {} MB", get_id(), mb);
        return true;

      } else if (resource_name == "pids") {
        size_t max_pids = std::stoull(value);
        container_->get_container().resources.max_open_files = max_pids;

        if (!container_->get_container().cgroup_path.empty()) {
          ossec::PidResources::set_pids_limit(
              container_->get_container().cgroup_path, max_pids);
        }

        spdlog::info("Container {} max pids set to {}", get_id(), max_pids);
        return true;

      } else if (resource_name == "apply") {
        spdlog::info("Applying resource changes for container {}", get_id());
        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to set resource {} to {}: {}", resource_name, value,
                    e.what());
    }

    return false;
  }

  bool set_memory_limit(const std::string &mb_str) {
    try {
      size_t mb = std::stoull(mb_str);
      size_t bytes = mb * 1024 * 1024;

      spdlog::info("Setting memory limit to {} MB ({} bytes) for container {}",
                   mb, bytes, get_id());

      // Обновляем ресурсы в контейнере
      // В реальной реализации нужно обновить
      // container_->get_container().resources.memory_capacity

      // Временно сохраняем в файл для демонстрации
      auto data_path = container_->get_container().data_path;
      std::string config_path = (data_path / "resource_config.json").string();
      std::ofstream config_file(config_path, std::ios::app);
      if (config_file) {
        config_file << "[" << time(nullptr) << "] memory_limit_mb=" << mb
                    << "\n";
        config_file.close();
      }

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to set memory limit: {}", e.what());
      return false;
    }
  }

  bool set_disk_quota(const std::string &mb_str) {
    try {
      size_t mb = std::stoull(mb_str);
      size_t bytes = mb * 1024 * 1024;

      spdlog::info("Setting disk quota to {} MB ({} bytes) for container {}",
                   mb, bytes, get_id());

      auto data_path = container_->get_container().data_path;
      std::string config_path = (data_path / "resource_config.json").string();
      std::ofstream config_file(config_path, std::ios::app);
      if (config_file) {
        config_file << "[" << time(nullptr) << "] disk_quota_mb=" << mb << "\n";
        config_file.close();
      }

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to set disk quota: {}", e.what());
      return false;
    }
  }

  bool set_pids_limit(const std::string &max_pids_str) {
    try {
      size_t max_pids = std::stoull(max_pids_str);

      spdlog::info("Setting max pids to {} for container {}", max_pids,
                   get_id());

      auto data_path = container_->get_container().data_path;
      std::string config_path = (data_path / "resource_config.json").string();
      std::ofstream config_file(config_path, std::ios::app);
      if (config_file) {
        config_file << "[" << time(nullptr) << "] max_pids=" << max_pids
                    << "\n";
        config_file.close();
      }

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to set pids limit: {}", e.what());
      return false;
    }
  }

  bool apply_resource_changes() {
    spdlog::info("Applying resource changes for container {}", get_id());

    auto data_path = container_->get_container().data_path;
    std::string config_path = (data_path / "resource_config.json").string();
    std::ofstream config_file(config_path, std::ios::app);
    if (config_file) {
      config_file << "[" << time(nullptr) << "] changes_applied=true\n";
      config_file.close();
    }

    return true;
  }

  std::string get_current_resources() const override {
    std::stringstream ss;

    if (container_) {
      auto cont = container_->get_container();

      ss << "=== Current Resource Limits ===\n\n";
      ss << "Memory: " << cont.resources.memory_capacity << " bytes ";
      ss << "(" << cont.resources.memory_capacity / (1024 * 1024) << " MB)\n";

      ss << "Disk: " << cont.resources.storage_quota << " bytes ";
      ss << "(" << cont.resources.storage_quota / (1024 * 1024) << " MB)\n";

      ss << "Max Processes/Files: " << cont.resources.max_open_files << "\n";

      ss << "\nChange with: echo 'VALUE' > /containers/" << get_id()
         << "/.resources/RESOURCE_NAME\n";
      ss << "Apply changes: echo 'apply' > /containers/" << get_id()
         << "/.resources/apply\n";
    }

    return ss.str();
  }

  std::string get_metrics() const override {
    std::stringstream ss;

    auto native = get_native_container();
    if (native) {
      auto cont = native->get_container();

      ss << "Memory Usage: ";
      if (!cont.cgroup_path.empty()) {
        ss << "monitored via cgroup\n";
      } else {
        ss << "not monitored\n";
      }

      ss << "Resource Limits:\n";
      ss << "  Memory: " << cont.resources.memory_capacity << " bytes\n";
      ss << "  Storage: " << cont.resources.storage_quota << " bytes\n";
      ss << "  Max Files: " << cont.resources.max_open_files << "\n";
    }

    return ss.str();
  }

  bool file_exists(const std::string &virtual_path) const override {
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

  std::string get_file_content(const std::string &virtual_path) const override {
    if (virtual_path.empty() || virtual_path == "/") {
      return "";
    }

    auto data_path = container_->get_container().data_path;

    std::string real_path = virtual_path;
    if (real_path[0] == '/') {
      real_path = real_path.substr(1);
    }

    auto full_path = data_path / real_path;

    spdlog::info("📖 get_file_content: FUSE='{}' -> REAL='{}'", virtual_path,
                 full_path.string());

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
      } else {
        spdlog::warn("File not found: {}", full_path.string());
      }
    } catch (const std::exception &e) {
      spdlog::error("Error reading file: {}", e.what());
    }

    return "";
  }

  bool is_directory(const std::string &virtual_path) const override {
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
      return is_dir;
    } catch (const std::exception &e) {
      spdlog::error("Error checking directory: {}", e.what());
      return false;
    }
  }
  bool add_file(const std::string &path, const std::string &content) override {
    auto data_path = container_->get_container().data_path;

    std::string normalized_path = path;
    if (!normalized_path.empty() && normalized_path[0] == '/') {
      normalized_path = normalized_path.substr(1);
    }

    auto full_path = data_path / normalized_path;

    try {
      std::filesystem::create_directories(full_path.parent_path());

      std::ofstream file(full_path);
      if (!file) {
        return false;
      }

      file << content;
      file.close();

      std::string search_path = "/" + normalized_path;

      auto result = search_->addFileImpl(search_path, content);
      if (!result.is_ok()) {
        spdlog::error("Failed to add file to search index: {} - {}",
                      search_path, result.error().what());
        return false;
      }

      search_->updateSemanticRelationships();

      auto rebuild_result = search_->rebuildIndexImpl();
      if (!rebuild_result.is_ok()) {
        spdlog::warn("Failed to rebuild container index: {}",
                     rebuild_result.error().what());
      }

      record_file_access(search_path, "write");

      return true;

    } catch (const std::exception &e) {
      spdlog::error("❌ Exception while adding file {}: {}", path, e.what());
      return false;
    }
  }

  bool rebuildIndex() override {
    auto rebuild_result = search_->rebuildIndexImpl();
    if (!rebuild_result.is_ok()) {
      spdlog::warn("Failed to rebuild container index: {}",
                   rebuild_result.error().what());
      return false;
    }

    return true;
  }

  bool remove_file(const std::string &path) override {
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

        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to remove file {}: {}", path, e.what());
    }

    return false;
  }

  void initialize_search() {
    auto files = list_files("/");
    size_t indexed_count = 0;

    for (const auto &file : files) {
      std::string file_path = "/" + file;
      std::string content = get_file_content(file_path);

      if (!content.empty()) {
        auto result = search_->addFileImpl(file_path, content);
        if (result.is_ok()) {
          indexed_count++;
          record_file_access(file_path, "read");
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
  semantic_search(const std::string &query, int limit = 10) override {
    record_search_query(query);

    auto result = search_->hybridSemanticSearch(query, limit);

    if (!result.is_ok()) {
      spdlog::debug("No semantic search results for query: '{}'", query);
      return {};
    }

    auto res = result.unwrap();

    if (res.empty()) {
      spdlog::debug("No semantic search results for query: '{}'", query);
      return {};
    }

    std::vector<std::pair<std::string, float>> file_paths;
    for (const auto &[file_path, score] : res) {
      file_paths.push_back({file_path, score});

      record_file_access(file_path, "semantic_search");
    }

    spdlog::debug("Semantic search found {} results", file_paths.size());
    return file_paths;
  }

  std::vector<std::string>
  search_files(const std::string &pattern) const override {
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
  enhanced_semantic_search(const std::string &query, int limit) override {
    record_search_query(query);

    auto result = search_->enhancedSemanticSearchImpl(query, limit);

    if (!result.is_ok()) {
      spdlog::warn("Enhanced search failed, falling back to basic search: {}",
                   result.error().what());
      return semantic_search(query, limit);
    }

    std::vector<std::pair<std::string, float>> file_paths;
    for (const auto &[file_path, score] : result.value()) {
      file_paths.push_back({file_path, score});
      record_file_access(file_path, "enhanced_search");
    }

    return file_paths;
  }

  std::vector<std::string> get_recommendations(const std::string &current_file,
                                               int limit) override {
    record_file_access(current_file, "recommendation_request");

    auto result = search_->getRecommendationsImpl(current_file);

    if (!result.is_ok()) {
      spdlog::debug("Failed to get recommendations: {}", result.error().what());

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

  std::vector<std::string> predict_next_files(int limit) override {
    auto result = search_->predictNextFilesImpl();

    if (!result.is_ok()) {
      spdlog::debug("Failed to predict next files: {}", result.error().what());
      return {};
    }

    auto predictions = result.value();
    spdlog::debug("Markov chain predicted {} next files", predictions.size());
    return predictions;
  }

  std::vector<std::string> get_semantic_hubs(int count = 5) override {
    auto result = search_->getSemanticHubsImpl(count);

    if (!result.is_ok()) {
      spdlog::debug("Failed to get semantic hubs: {}", result.error().what());
      return {};
    }

    auto hubs = result.value();
    spdlog::debug("Found {} semantic hubs", hubs.size());
    return hubs;
  }

  std::string classify_file(const std::string &file_path) override {
    std::string category = search_->classifyFileCategoryImpl(file_path);
    spdlog::debug("File {} classified as: {}", file_path, category);
    return category;
  }

  void record_file_access(const std::string &file_path,
                          const std::string &operation = "read") {
    auto result = search_->recordFileAccessImpl(file_path, operation);
    if (!result.is_ok()) {
      spdlog::debug("Failed to record file access: {} - {}", file_path,
                    result.error().what());
    } else {
      spdlog::trace("Recorded {} access to: {}", operation, file_path);
    }
  }

  void record_search_query(const std::string &query) override {
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

  bool update_all_embeddings() override {
    auto files = list_files(container_->get_container().data_path.string());
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

  std::string getSearch_info() const override {
    auto file_count = search_->getIndexedFilesCountImpl();
    auto recent_queries = search_->getRecentQueriesCountImpl();

    std::stringstream ss;
    ss << "Search Info for Container " << get_id() << ":\n";
    ss << "  Indexed Files: " << (file_count.is_ok() ? file_count.value() : 0)
       << "\n";
    ss << "  Recent Queries: "
       << (recent_queries.is_ok() ? recent_queries.value() : 0) << "\n";
    ss << "  Embedder: " << search_->getEmbedderInfoImpl() << "\n";

    return ss.str();
  }

  bool is_available() const override {
    return container_ && container_->is_running();
  }

  size_t get_size() const override {
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

  std::string get_status() const override {
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

} // namespace owl

#endif