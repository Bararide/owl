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

  std::vector<std::string> list_files(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;
    std::vector<std::string> files;

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_directory(full_path)) {

        for (const auto &entry :
             std::filesystem::directory_iterator(full_path)) {
          std::string filename = entry.path().filename().string();

          files.push_back(filename);
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("Error listing files in {}: {}", full_path.string(),
                    e.what());
    }

    return files;
  }

  std::string get_file_content(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path);
        return std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
      }
    } catch (const std::exception &e) {
      spdlog::debug("Error reading file {}: {}", path, e.what());
    }

    return "";
  }

  bool add_file(const std::string &path, const std::string &content) override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      std::filesystem::create_directories(full_path.parent_path());
      std::ofstream file(full_path);
      if (file) {
        file << content;
        file.close();

        auto result = search_->addFileImpl(path, content);
        if (!result.is_ok()) {
          spdlog::warn("Failed to add file to search index: {} - {}", path,
                       result.error().what());
          return false;
        }

        search_->updateSemanticRelationships();

        record_file_access(path, "write");

        spdlog::info("File added successfully: {}", path);
        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to add file {}: {}", path, e.what());
    }

    return false;
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

        spdlog::info("File removed successfully: {}", path);
        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to remove file {}: {}", path, e.what());
    }

    return false;
  }

  void initialize_search() {
    spdlog::info("Initializing search for container: {}", get_id());

    auto files = list_files(container_->get_container().data_path.string());
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

    spdlog::info("Search initialized for container {} with {}/{} files indexed",
                 get_id(), indexed_count, files.size());
  }

  void initialize_markov_chain() {
    spdlog::info("Initializing Markov chain for container: {}", get_id());

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

    spdlog::info("Markov chain initialized: {} states, {} edges",
                 hmm_model->get_state_count(),
                 semantic_graph->get_edge_count());

    if (!hubs.empty()) {
      spdlog::info("Top semantic hub: {} (score: {:.3f})", hubs[0].first,
                   hubs[0].second);
    }
  }

  bool file_exists(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;
    return std::filesystem::exists(full_path);
  }

  std::vector<std::string> semantic_search(const std::string &query,
                                           int limit = 10) override {
    spdlog::debug("Semantic search in container {}: '{}'", get_id(), query);

    record_search_query(query);

    auto result = search_->semanticSearchImpl(query, limit);

    if (result.empty()) {
      spdlog::debug("No semantic search results for query: '{}'", query);
      return {};
    }

    std::vector<std::string> file_paths;
    for (const auto &[file_path, score] : result) {
      file_paths.push_back(file_path);

      record_file_access(file_path, "semantic_search");
    }

    spdlog::debug("Semantic search found {} results", file_paths.size());
    return file_paths;
  }

  std::vector<std::string>
  search_files(const std::string &pattern) const override {
    spdlog::debug("Pattern search in container {}: '{}'", get_id(), pattern);

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

    spdlog::debug("Pattern search found {} results", results.size());
    return results;
  }

  std::vector<std::string> enhanced_semantic_search(const std::string &query,
                                                    int limit) override {
    spdlog::debug("Enhanced semantic search in container {}: '{}'", get_id(),
                  query);

    record_search_query(query);

    auto result = search_->enhancedSemanticSearchImpl(query, limit);

    if (!result.is_ok()) {
      spdlog::warn("Enhanced search failed, falling back to basic search: {}",
                   result.error().what());
      return semantic_search(query, limit);
    }

    std::vector<std::string> file_paths;
    for (const auto &[file_path, score] : result.value()) {
      file_paths.push_back(file_path);
      record_file_access(file_path, "enhanced_search");
    }

    spdlog::debug("Enhanced semantic search found {} results",
                  file_paths.size());
    return file_paths;
  }

  std::vector<std::string> get_recommendations(const std::string &current_file,
                                               int limit) override {
    spdlog::debug("Getting recommendations for file: {}", current_file);

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

    spdlog::debug("Found {} recommendations", recommendations.size());
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
    spdlog::info("Updating embeddings for all files in container: {}",
                 get_id());

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

    spdlog::info("Updated embeddings for {}/{} files", updated_count,
                 files.size());
    return updated_count > 0;
  }

  std::string get_search_info() const override {
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

} // namespace owl::vectorfs

#endif