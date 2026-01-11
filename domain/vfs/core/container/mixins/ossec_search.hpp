#ifndef OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_SEARCH
#define OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_SEARCH

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <infrastructure/result.hpp>
#include <spdlog/spdlog.h>

#include "ossec_fs_helpers.hpp"

namespace owl {

template <typename Derived>
class OssecSearchMixin : 
                         public SearchableContainer<Derived> {
public:
  using Error = std::runtime_error;

  core::Result<std::vector<std::pair<std::string, float>>>
  semanticSearch(const std::string &query, int limit) {
    recordSearchQuery(query);

    auto &search = derived().search();

    auto r = search.hybridSemanticSearch(query, limit);
    if (!r.is_ok()) {
      return core::Result<std::vector<std::pair<std::string, float>>,
                          Error>::Error(Error("semanticSearch error: " +
                                              std::string(r.error().what())));
    }

    std::vector<std::pair<std::string, float>> out;
    const auto &res = r.value();
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

    auto &search = derived().search();
    auto r = search.enhancedSemanticSearch(query, limit);
    if (!r.is_ok()) {
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

    auto &search = derived().search();
    auto r = search.getRecommendations(current_file);
    if (!r.is_ok()) {
      auto predictions = search.predictNextFiles();
      if (predictions.is_ok()) {
        auto v = predictions.value();
        if (static_cast<int>(v.size()) > limit) {
          v.resize(limit);
        }
        return core::Result<std::vector<std::string>, Error>::Ok(std::move(v));
      }
      return core::Result<std::vector<std::string>, Error>::Error(
          Error("getRecommendations failed: " + std::string(r.error().what())));
    }

    auto recommendations = r.value();
    if (static_cast<int>(recommendations.size()) > limit) {
      recommendations.resize(limit);
    }
    return core::Result<std::vector<std::string>, Error>::Ok(
        std::move(recommendations));
  }

  core::Result<std::vector<std::string>> predictNextFiles(int /*limit*/) {
    auto &search = derived().search();
    auto r = search.predictNextFiles();
    if (!r.is_ok()) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error("predictNextFiles error: " + std::string(r.error().what())));
    }
    return core::Result<std::vector<std::string>, Error>::Ok(r.value());
  }

  core::Result<std::vector<std::string>> getSemanticHubs(int count) {
    auto &search = derived().search();
    auto r = search.getSemanticHubs(count);
    if (!r.is_ok()) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error("getSemanticHubs error: " + std::string(r.error().what())));
    }
    return core::Result<std::vector<std::string>, Error>::Ok(r.value());
  }

  core::Result<std::string> classifyFile(const std::string &file_path) {
    auto &search = derived().search();
    std::string category = search.classifyFileCategory(file_path);
    return core::Result<std::string, Error>::Ok(std::move(category));
  }

  core::Result<void> updateAllEmbeddings() {
    auto files_res = derived().listFiles("/");
    if (!files_res.is_ok()) {
      return core::Result<void, Error>::Error(files_res.error());
    }

    auto &search = derived().search();
    const auto &files = files_res.value();

    for (const auto &file : files) {
      const auto file_path = "/" + file;
      auto r = search.updateEmbedding(file_path);
      if (!r.is_ok()) {
        spdlog::warn("Failed to update embedding for {}: {}", file_path,
                     r.error().what());
      }
    }

    rebuildSearchIndexWithRelationships();
    return core::Result<void, Error>::Ok();
  }

  core::Result<std::string> getSearchInfo() const {
    auto &search = derived().search();

    auto file_count = search.getIndexedFilesCount();
    auto recent_queries = search.getRecentQueriesCount();

    std::stringstream ss;
    ss << "Search Info for Container " << derived().getId() << ":\n";
    ss << "  Indexed Files: " << (file_count.is_ok() ? file_count.value() : 0)
       << "\n";
    ss << "  Recent Queries: "
       << (recent_queries.is_ok() ? recent_queries.value() : 0) << "\n";
    ss << "  Embedder: " << search.getEmbedderInfo() << "\n";

    return core::Result<std::string, Error>::Ok(ss.str());
  }

  core::Result<void> recordSearchQuery(const std::string &query) {
    auto &search = derived().search();
    auto r = search.getRecentQueries();
    if (!r.is_ok()) {
      return core::Result<void, Error>::Error(
          Error("recordSearchQuery error: " + std::string(r.error().what())));
    }
    spdlog::trace("Recorded search query: {}", query);
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> indexFileInSearch(const std::string &virtual_path,
                                       const std::string &content,
                                       const std::string &access_reason) {
    auto &search = derived().search();

    auto r = search.addFile(virtual_path, content);
    if (!r.is_ok()) {
      return core::Result<void, Error>::Error(
          Error("search::addFile failed: " + std::string(r.error().what())));
    }

    rebuildSearchIndexWithRelationships();
    recordFileAccess(virtual_path, access_reason);

    return core::Result<void, Error>::Ok();
  }

  core::Result<void> removeFileFromSearch(const std::string &virtual_path) {
    auto &search = derived().search();

    auto r = search.removeFile(virtual_path);
    if (!r.is_ok()) {
      spdlog::warn("Failed to remove from index: {}", r.error().what());
    }

    search.updateSemanticRelationships();
    return core::Result<void, Error>::Ok();
  }

  void rebuildSearchIndexWithRelationships() {
    auto &search = derived().search();

    search.updateSemanticRelationships();
    auto rebuild = search.rebuildIndex();
    if (!rebuild.is_ok()) {
      spdlog::warn("Failed to rebuild index: {}", rebuild.error().what());
    }
  }

  void initializeSearchIndexFromFs() {
    auto files_res = derived().listFiles("/");
    if (!files_res.is_ok()) {
      spdlog::warn("initializeSearch: {}", files_res.error().what());
      return;
    }

    auto &search = derived().search();
    const auto &files = files_res.value();

    for (const auto &file : files) {
      const std::string path = "/" + file;
      auto content_res = derived().getFileContent(path);
      if (!content_res.is_ok()) {
        spdlog::warn("Failed to get content for {}: {}", path,
                     content_res.error().what());
        continue;
      }

      auto r = search.addFile(path, content_res.value());
      if (r.is_ok()) {
        recordFileAccess(path, "read");
      } else {
        spdlog::warn("Failed to index file {}: {}", path, r.error().what());
      }
    }

    rebuildSearchIndexWithRelationships();
  }

protected:
  void recordFileAccess(const std::string &file_path,
                        const std::string &operation) {
    auto &search = derived().search();
    auto r = search.recordFileAccessImpl(file_path, operation);
    if (!r.is_ok()) {
      spdlog::debug("Failed to record file access: {} - {}", file_path,
                    r.error().what());
    }
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_SEARCH