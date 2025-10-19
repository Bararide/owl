#include "vectorfs.hpp"

namespace owl::vectorfs {

std::string
VectorFS::generate_enhanced_search_result(const std::string &query) {
  auto result = search_.recordFileAccessImpl("/.search/" + query, "search");

  if (result.is_ok()) {
    spdlog::info("Success record file access");
  } else {
    spdlog::error("Record file access fault");
  }

  auto search_results = search_.enhancedSemanticSearchImpl(query, 5);
  auto recommendations = search_.getRecommendationsImpl(query);
  auto predicted_next = search_.predictNextFilesImpl();
  auto hubs_result = search_.getSemanticHubsImpl(3);

  std::stringstream ss;
  ss << "=== Enhanced Semantic Search Results ===\n";
  ss << "Query: " << query << "\n\n";

  if (!search_results.is_ok() || search_results.value().empty()) {
    ss << "No results found\n";
  } else {
    ss << "📊 Search Results (with PageRank):\n";
    for (const auto &[file_path, score] : search_results.value()) {
      auto it = virtual_files.find(file_path);
      ss << "📄 " << file_path << " (score: " << score << ")\n";
      if (it != virtual_files.end()) {
        ss << "   Content: "
           << (it->second.content.size() > 50
                   ? it->second.content.substr(0, 50) + "..."
                   : it->second.content)
           << "\n";

        std::string category = search_.classifyFileCategoryImpl(file_path);
        ss << "   Category: " << category << "\n";
      }
      ss << "\n";
    }
  }

  if (recommendations.is_ok() && !recommendations.value().empty()) {
    ss << "🎯 Recommended Files:\n";
    for (const auto &rec : recommendations.value()) {
      ss << "   → " << rec << "\n";
    }
    ss << "\n";
  }

  if (predicted_next.is_ok() && !predicted_next.value().empty()) {
    ss << "🔮 Predicted Next Files:\n";
    for (const auto &pred : predicted_next.value()) {
      ss << "   ↗ " << pred << "\n";
    }
    ss << "\n";
  }

  if (hubs_result.is_ok() && !hubs_result.value().empty()) {
    ss << "🌐 Semantic Hubs:\n";
    for (const auto &hub : hubs_result.value()) {
      ss << "   ⭐ " << hub << "\n";
    }
    ss << "\n";
  }

  ss << "=== Analytics ===\n";

  auto indexed_count = search_.getIndexedFilesCountImpl();
  if (indexed_count.is_ok()) {
    ss << "Total indexed files: " << indexed_count.value() << "\n";
  } else {
    ss << "Total indexed files: unknown\n";
  }
  ss << "Recent access patterns: " << "available in search model" << "\n";

  return ss.str();
}

std::string VectorFS::generate_search_result(const std::string &query) {
  spdlog::info("Processing search query: {}", query);

  auto search_results = search_.semanticSearchImpl(query, 5);

  std::stringstream ss;
  ss << "=== Semantic Search Results ===\n";
  ss << "Query: " << query << "\n\n";

  if (!search_results.is_ok() || search_results.value().empty()) {
    ss << "No results found\n";

    auto indexed_count = search_.getIndexedFilesCountImpl();
    ss << "Indexed files: ";
    if (indexed_count.is_ok()) {
      ss << indexed_count.value();
    } else {
      ss << "unknown";
    }
    ss << "\n";

    if (indexed_count.is_ok() && indexed_count.value() == 0) {
      ss << "Hint: Create some files with content first!\n";
    }
  } else {
    ss << "Found " << search_results.value().size() << " results:\n\n";
    for (const auto &[file_path, score] : search_results.value()) {
      auto it = virtual_files.find(file_path);
      ss << "📄 " << file_path << " (score: " << score << ")\n";
      if (it != virtual_files.end()) {
        ss << "   Content: "
           << (it->second.content.size() > 50
                   ? it->second.content.substr(0, 50) + "..."
                   : it->second.content)
           << "\n\n";
      }
    }
  }

  ss << "\n=== Search Info ===\n";

  auto indexed_count = search_.getIndexedFilesCountImpl();
  ss << "Total indexed files: ";
  if (indexed_count.is_ok()) {
    ss << indexed_count.value();
  } else {
    ss << "unknown";
  }
  ss << "\n";

  auto embedder_info = search_.getEmbedderInfoImpl();
  ss << "Embedder: " << embedder_info << "\n";

  return ss.str();
}

} // namespace owl::vectorfs