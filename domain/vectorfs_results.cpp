#include "vectorfs.hpp"

namespace owl::vectorfs {
std::string
VectorFS::generate_enhanced_search_result(const std::string &query) {
  record_file_access("/.search/" + query, "search");

  auto results = enhanced_semantic_search(query, 5);
  auto recommendations = get_recommendations_for_query(query);
  auto predicted_next = predict_next_files();
  auto hubs = semantic_graph->get_semantic_hubs(3);

  std::stringstream ss;
  ss << "=== Enhanced Semantic Search Results ===\n";
  ss << "Query: " << query << "\n\n";

  if (results.empty()) {
    ss << "No results found\n";
  } else {
    ss << "📊 Search Results (with PageRank):\n";
    for (const auto &[file_path, score] : results) {
      auto it = virtual_files.find(file_path);
      ss << "📄 " << file_path << " (score: " << score << ")\n";
      if (it != virtual_files.end()) {
        ss << "   Content: "
           << (it->second.content.size() > 50
                   ? it->second.content.substr(0, 50) + "..."
                   : it->second.content)
           << "\n";

        std::string category =
            hmm_model->classify_file_category(file_path, recent_queries);
        ss << "   Category: " << category << "\n";
      }
      ss << "\n";
    }
  }

  if (!recommendations.empty()) {
    ss << "🎯 Recommended Files:\n";
    for (const auto &rec : recommendations) {
      ss << "   → " << rec << "\n";
    }
    ss << "\n";
  }

  if (!predicted_next.empty()) {
    ss << "🔮 Predicted Next Files:\n";
    for (const auto &pred : predicted_next) {
      ss << "   ↗ " << pred << "\n";
    }
    ss << "\n";
  }

  if (!hubs.empty()) {
    ss << "🌐 Semantic Hubs:\n";
    for (const auto &hub : hubs) {
      ss << "   ⭐ " << hub << "\n";
    }
    ss << "\n";
  }

  ss << "=== Analytics ===\n";
  ss << "Total indexed files: " << index_to_path.size() << "\n";
  ss << "Recent access patterns: " << recent_queries.size() << "\n";

  return ss.str();
}

std::vector<std::pair<std::string, float>>
VectorFS::enhanced_semantic_search(const std::string &query, int k) {
  auto base_results = semantic_search(query, k * 2);

  auto ranking = semantic_graph->random_walk_ranking();

  std::map<std::string, double> combined_scores;

  for (const auto &[file_path, sem_score] : base_results) {
    combined_scores[file_path] = sem_score;
  }

  for (const auto &[file_path, pagerank_score] : ranking) {
    if (combined_scores.find(file_path) != combined_scores.end()) {
      combined_scores[file_path] *= (1.0 + pagerank_score);
    }
  }

  std::vector<std::pair<std::string, float>> final_results;
  for (const auto &[file_path, score] : combined_scores) {
    final_results.emplace_back(file_path, static_cast<float>(score));
  }

  std::sort(final_results.begin(), final_results.end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });

  if (final_results.size() > k) {
    final_results.resize(k);
  }

  return final_results;
}

std::string VectorFS::generate_search_result(const std::string &query) {
  spdlog::info("Processing search query: {}", query);
  auto results = semantic_search(query, 5);
  std::stringstream ss;
  ss << "=== Semantic Search Results ===\n";
  ss << "Query: " << query << "\n\n";
  if (results.empty()) {
    ss << "No results found\n";
    ss << "Indexed files: " << index_to_path.size() << "\n";
    if (index_to_path.empty()) {
      ss << "Hint: Create some files with content first!\n";
    }
  } else {
    ss << "Found " << results.size() << " results:\n\n";
    for (const auto &[file_path, score] : results) {
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
  ss << "Total indexed files: " << index_to_path.size() << "\n";
  ss << "Embedder dimension: "
     << std::visit(
            [](const auto &embedder_ptr) {
              return embedder_ptr->getDimension();
            },
            embedder_)
     << "\n";
  return ss.str();
}
} // namespace owl::vectorfs