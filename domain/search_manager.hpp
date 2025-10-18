#ifndef SEARCH_MANAGER_HPP
#define SEARCH_MANAGER_HPP

#include "markov.hpp"
#include "utils/quantization.hpp"
#include <embedded/emdedded_manager.hpp>
#include <faiss/IndexFlat.h>
#include <functional>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace owl::vectorfs {

struct SearchFileInfo {
  std::string content;
  std::vector<float> embedding;
  bool embedding_updated = false;
  std::vector<uint8_t> sq_codes;
  std::vector<uint8_t> pq_codes;
};

class SearchManager {
private:
  std::unique_ptr<faiss::IndexFlatL2> faiss_index;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index_quantized;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer;

  bool index_needs_rebuild;
  bool use_quantization;
  std::map<faiss::idx_t, std::string> index_to_path;

  std::map<std::string, SearchFileInfo> file_store_;

  std::unique_ptr<markov::SemanticGraph> semantic_graph;
  std::unique_ptr<markov::HiddenMarkovModel> hmm_model;
  std::vector<std::string> recent_queries;

  EmbedderManager<> embedder_;

  std::function<std::string(const std::string &)> content_provider_;

public:
  SearchManager(const std::string &model_path)
      : index_needs_rebuild(true), use_quantization(false) {
    semantic_graph = std::make_unique<markov::SemanticGraph>();
    hmm_model = std::make_unique<markov::HiddenMarkovModel>();

    embedder_.set(model_path);

    hmm_model->add_state("code");
    hmm_model->add_state("document");
    hmm_model->add_state("config");
    hmm_model->add_state("test");
    hmm_model->add_state("misc");
  }

  [[nodiscard]] const EmbedderManager<> &get_embedder_manager() noexcept {
    return embedder_;
  }

  bool initialize(const std::string &model_path,
                  bool use_quantization = false) {
    try {
      this->use_quantization = use_quantization;

      if (use_quantization) {
        sq_quantizer = std::make_unique<utils::ScalarQuantizer>();
        pq_quantizer = std::make_unique<utils::ProductQuantizer>(8, 256);
      }

      spdlog::info("SearchManager initialized with quantization: {}",
                   use_quantization);
      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to initialize SearchManager: {}", e.what());
      return false;
    }
  }

  void set_content_provider(
      std::function<std::string(const std::string &)> provider) {
    content_provider_ = std::move(provider);
  }

  void add_file(const std::string &path, const std::string &content) {
    file_store_[path] = {content, {}, false, {}, {}};
    update_embedding(path);
    index_needs_rebuild = true;
  }

  void remove_file(const std::string &path) {
    file_store_.erase(path);

    for (auto it = index_to_path.begin(); it != index_to_path.end();) {
      if (it->second == path) {
        it = index_to_path.erase(it);
      } else {
        ++it;
      }
    }
    index_needs_rebuild = true;
  }

  void update_file(const std::string &path, const std::string &content) {
    auto it = file_store_.find(path);
    if (it != file_store_.end()) {
      it->second.content = content;
      it->second.embedding_updated = false;
      update_embedding(path);
      index_needs_rebuild = true;
    }
  }

  std::vector<std::pair<std::string, float>>
  semantic_search(const std::string &query, int k) {
    std::vector<std::pair<std::string, float>> results;

    if (!embedder_.embedder().is_ok()) {
      spdlog::error("Embedder not initialized");
      return results;
    }

    rebuild_index();

    if (index_to_path.empty()) {
      spdlog::warn("No files indexed for search");
      return results;
    }

    std::string normalized_query = normalize_text(query);
    auto embedding_result =
        embedder_.embedder().value().getSentenceEmbedding(normalized_query);

    if (!embedding_result.is_ok()) {
      spdlog::error("Failed to get embedding for query: {}", query);
      return results;
    }

    std::vector<float> query_embedding = embedding_result.value();
    std::vector<faiss::idx_t> I(k);
    std::vector<float> D(k);

    if (use_quantization && faiss_index_quantized) {
      if (pq_quantizer && pq_quantizer->is_trained()) {
        std::vector<uint8_t> query_codes =
            pq_quantizer->encode(query_embedding);
        pq_quantizer->precompute_query_tables(query_embedding);

        std::vector<std::pair<float, std::string>> scored_results;
        for (const auto &[idx, path] : index_to_path) {
          const auto &file_info = file_store_[path];
          if (!file_info.pq_codes.empty()) {
            float dist = pq_quantizer->asymmetric_distance(file_info.pq_codes);
            scored_results.emplace_back(dist, path);
          }
        }
        std::sort(scored_results.begin(), scored_results.end());
        for (int i = 0; i < std::min(k, (int)scored_results.size()); ++i) {
          results.push_back(
              {scored_results[i].second, scored_results[i].first});
        }
      }
    } else if (faiss_index) {
      std::vector<float> all_embeddings;
      std::vector<std::string> indexed_paths;

      for (const auto &[path, file_info] : file_store_) {
        if (file_info.embedding_updated && !file_info.embedding.empty()) {
          all_embeddings.insert(all_embeddings.end(),
                                file_info.embedding.begin(),
                                file_info.embedding.end());
          indexed_paths.push_back(path);
        }
      }

      if (!all_embeddings.empty()) {
        faiss_index->search(1, query_embedding.data(), k, D.data(), I.data());
        for (int i = 0; i < k; ++i) {
          if (I[i] >= 0 && I[i] < indexed_paths.size()) {
            results.push_back({indexed_paths[I[i]], D[i]});
          }
        }
      }
    }

    spdlog::debug("Semantic search for '{}' returned {} results", query,
                  results.size());
    return results;
  }

  std::vector<std::pair<std::string, float>>
  enhanced_semantic_search(const std::string &query, int k) {
    auto base_results = semantic_search(query, k * 2);

    if (base_results.empty()) {
      return {};
    }

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

  std::vector<std::string>
  get_recommendations_for_query(const std::string &query) {
    auto search_results = semantic_search(query, 1);
    if (search_results.empty())
      return {};

    std::string current_file = search_results[0].first;
    return semantic_graph->get_recommendations(current_file, 3);
  }

  std::vector<std::string> predict_next_files() {
    return hmm_model->predict_next_files(recent_queries, 3);
  }

  std::vector<std::string> get_semantic_hubs(int count) {
    return semantic_graph->get_semantic_hubs(count);
  }

  size_t get_indexed_files_count() const { return index_to_path.size(); }

  std::string get_embedder_info() {
    if (embedder_.embedder().is_ok()) {
      return fmt::format("Model: {}, Dimension: {}",
                         embedder_.embedder().value().getModelName(),
                         embedder_.embedder().value().getDimension());
    }
    return "Embedder not initialized";
  }

  void record_file_access(const std::string &file_path,
                          const std::string &operation = "read") {
    semantic_graph->record_access(file_path, operation);

    recent_queries.push_back(file_path);
    if (recent_queries.size() > 50) {
      recent_queries.erase(recent_queries.begin(), recent_queries.begin() + 10);
    }

    if (recent_queries.size() >= 10) {
      std::vector<std::string> sequence(recent_queries.end() - 10,
                                        recent_queries.end());
      hmm_model->add_sequence(sequence);
    }
  }

  void rebuild_index() {
    if (!index_needs_rebuild)
      return;

    std::vector<float> all_embeddings;
    index_to_path.clear();

    faiss::idx_t idx = 0;
    for (const auto &[path, file_info] : file_store_) {
      if (file_info.embedding_updated && !file_info.embedding.empty()) {
        all_embeddings.insert(all_embeddings.end(), file_info.embedding.begin(),
                              file_info.embedding.end());
        index_to_path[idx] = path;
        idx++;
      }
    }

    if (!all_embeddings.empty()) {
      if (use_quantization) {
        if (!faiss_index_quantized) {
          int dimension = embedder_.embedder().value().getDimension();
          faiss_index_quantized =
              std::make_unique<faiss::IndexFlatL2>(dimension);
        }
      } else {
        if (!faiss_index) {
          int dimension = embedder_.embedder().value().getDimension();
          faiss_index = std::make_unique<faiss::IndexFlatL2>(dimension);
        }
        faiss_index->reset();
        faiss_index->add(all_embeddings.size() /
                             embedder_.embedder().value().getDimension(),
                         all_embeddings.data());
      }
    }

    index_needs_rebuild = false;
    spdlog::info("Search index rebuilt with {} files", index_to_path.size());
  }

private:
  void update_embedding(const std::string &path) {
    auto it = file_store_.find(path);
    if (it == file_store_.end())
      return;

    auto &file_info = it->second;
    if (!file_info.content.empty()) {
      std::string normalized_content = normalize_text(file_info.content);
      auto embedding_result =
          embedder_.embedder().value().getSentenceEmbedding(normalized_content);

      if (embedding_result.is_ok()) {
        file_info.embedding = embedding_result.value();
        file_info.embedding_updated = true;

        if (use_quantization && sq_quantizer && sq_quantizer->is_trained()) {
          file_info.sq_codes = sq_quantizer->quantize(file_info.embedding);
        }
        if (use_quantization && pq_quantizer && pq_quantizer->is_trained()) {
          file_info.pq_codes = pq_quantizer->encode(file_info.embedding);
        }
        index_needs_rebuild = true;
        spdlog::debug("Updated embedding for file: {}", path);
      }
    }
  }

  void update_semantic_relationships() {
    for (const auto &[path1, file1] : file_store_) {
      if (!file1.embedding_updated || file1.embedding.empty())
        continue;

      for (const auto &[path2, file2] : file_store_) {
        if (path1 == path2 || !file2.embedding_updated ||
            file2.embedding.empty())
          continue;

        double similarity =
            calculate_cosine_similarity(file1.embedding, file2.embedding);

        if (similarity > 0.3) {
          semantic_graph->add_edge(path1, path2, similarity);
        }
      }
    }

    spdlog::info("Updated semantic relationships in graph");
  }

  void update_models() {
    spdlog::info("Updating Random Walk rankings and HMM model");

    update_semantic_relationships();
    semantic_graph->random_walk_ranking();
    hmm_model->train();
  }

  void train_quantizers(const std::vector<float> &embeddings, size_t dim) {
    if (embeddings.empty())
      return;

    std::vector<std::vector<float>> training_data;
    size_t num_vectors = embeddings.size() / dim;

    for (size_t i = 0; i < num_vectors; i++) {
      training_data.emplace_back(embeddings.begin() + i * dim,
                                 embeddings.begin() + (i + 1) * dim);
    }

    sq_quantizer->train(training_data, dim);
    pq_quantizer->train(training_data, dim);

    spdlog::info("Quantizers trained with {} vectors", num_vectors);
  }

  std::string normalize_text(const std::string &text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
  }

  double calculate_cosine_similarity(const std::vector<float> &a,
                                     const std::vector<float> &b) {
    if (a.size() != b.size())
      return 0.0;

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
      dot_product += a[i] * b[i];
      norm_a += a[i] * a[i];
      norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0 || norm_b == 0.0)
      return 0.0;

    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
  }
};

} // namespace owl::vectorfs

#endif