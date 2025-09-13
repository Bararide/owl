#ifndef VECTORFS_HPP
#define VECTORFS_HPP

#define FUSE_USE_VERSION 31

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <cstring>
#include <fstream>
#include <fuse3/fuse.h>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <faiss/IndexFlat.h>
#include <fasttext.h>
#include <spdlog/spdlog.h>

#include "embedded/embedded_base.hpp"
#include "embedded/embedded_fasttext.hpp"
#include "file/fileinfo.hpp"
#include "infrastructure/measure.hpp"
#include "markov.hpp"

namespace vfs::vectorfs {

using EmbedderVariant =
    std::variant<std::unique_ptr<embedded::FastTextEmbedder>>;

using idx_t = faiss::idx_t;

class VectorFS {
private:
  std::map<std::string, fileinfo::FileInfo> virtual_files;
  std::set<std::string> virtual_dirs;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index_quantized;
  std::unique_ptr<fileinfo::ScalarQuantizer> sq_quantizer;
  std::unique_ptr<fileinfo::ProductQuantizer> pq_quantizer;
  bool index_needs_rebuild;
  bool use_quantization;
  std::map<idx_t, std::string> index_to_path;
  std::map<std::string, std::string> search_results_cache;

  std::unique_ptr<markov::SemanticGraph> semantic_graph;
  std::unique_ptr<markov::HiddenMarkovModel> hmm_model;
  std::vector<std::string> recent_queries;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update;

  std::map<std::string, std::vector<std::string>> predictive_cache;

  EmbedderVariant embedder_;
  std::string model_type_;

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

  void update_models() {
    spdlog::info("Updating Random Walk rankings and HMM model");

    update_semantic_relationships();
    semantic_graph->random_walk_ranking();
    hmm_model->train();

    update_predictive_cache();
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

  void update_predictive_cache() {
    predictive_cache.clear();

    for (const auto &[file_path, _] : virtual_files) {
      auto recs = semantic_graph->get_recommendations(file_path, 5);
      if (!recs.empty()) {
        predictive_cache[file_path] = recs;
      }
    }

    spdlog::debug("Updated predictive cache with {} entries",
                  predictive_cache.size());
  }

  std::string url_decode(const std::string &str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
      if (str[i] == '%' && i + 2 < str.size()) {
        int value;
        std::istringstream iss(str.substr(i + 1, 2));
        if (iss >> std::hex >> value) {
          result += static_cast<char>(value);
          i += 2;
        } else {
          result += str[i];
        }
      } else if (str[i] == '+') {
        result += ' ';
      } else {
        result += str[i];
      }
    }

    return result;
  }

  void update_embedding(const std::string &path) {
    auto it = virtual_files.find(path);
    if (it == virtual_files.end())
      return;
    if (!it->second.content.empty()) {
      std::string normalized_content = normalize_text(it->second.content);
      it->second.embedding = std::visit(
          [&normalized_content](auto &embedder_ptr) {
            return embedder_ptr->getSentenceEmbedding(normalized_content);
          },
          embedder_);
      it->second.embedding_updated = true;
      if (use_quantization && sq_quantizer && sq_quantizer->is_trained()) {
        it->second.sq_codes = sq_quantizer->quantize(it->second.embedding);
      }
      if (use_quantization && pq_quantizer && pq_quantizer->is_trained()) {
        it->second.pq_codes = pq_quantizer->encode(it->second.embedding);
      }
      index_needs_rebuild = true;
      spdlog::debug("Updated embedding for file: {}", path);
    }
  }

  std::string generate_search_result(const std::string &query) {
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

public:
  VectorFS()
      : virtual_dirs({"/"}), index_needs_rebuild(true),
        use_quantization(false) {
    semantic_graph = std::make_unique<markov::SemanticGraph>();
    hmm_model = std::make_unique<markov::HiddenMarkovModel>();
    last_ranking_update = std::chrono::steady_clock::now();

    hmm_model->add_state("code");
    hmm_model->add_state("document");
    hmm_model->add_state("config");
    hmm_model->add_state("test");
    hmm_model->add_state("misc");
  }

  int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
  int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
              struct fuse_file_info *fi, enum fuse_readdir_flags flags);
  int rmdir(const char *path);
  int mkdir(const char *path, mode_t mode);
  int read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi);
  int create(const char *path, mode_t mode, struct fuse_file_info *fi);
  int utimens(const char *path, const struct timespec tv[2],
              struct fuse_file_info *fi);
  int write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi);
  int unlink(const char *path);
  int setxattr(const char *path, const char *name, const char *value,
               size_t size, int flags);
  int getxattr(const char *path, const char *name, char *value, size_t size);
  int listxattr(const char *path, char *list, size_t size);
  int open(const char *path, struct fuse_file_info *fi);

  void test_markov_chains();

  void update_semantic_relationships() {
    for (const auto &[path1, file1] : virtual_files) {
      if (!file1.embedding_updated || file1.embedding.empty())
        continue;

      for (const auto &[path2, file2] : virtual_files) {
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

    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(
                                 now - last_ranking_update)
                                 .count();

    if (time_since_update > 5) {
      update_models();
      last_ranking_update = now;
    }
  }

  std::vector<std::pair<std::string, float>>
  enhanced_semantic_search(const std::string &query, int k) {
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

  std::string generate_enhanced_search_result(const std::string &query) {
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

  ~VectorFS() = default;

  std::map<std::string, fileinfo::FileInfo> &get_virtual_files() {
    return virtual_files;
  }

  void rebuild_index() {
    if (!index_needs_rebuild)
      return;
    spdlog::info("Rebuilding vector index (quantization: {})",
                 use_quantization);
    index_to_path.clear();
    std::vector<float> all_embeddings;
    std::vector<std::string> indexed_paths;
    idx_t idx = 0;
    for (const auto &[path, file_info] : virtual_files) {
      if (file_info.embedding_updated && !file_info.embedding.empty()) {
        all_embeddings.insert(all_embeddings.end(), file_info.embedding.begin(),
                              file_info.embedding.end());
        index_to_path[idx] = path;
        indexed_paths.push_back(path);
        idx++;
      }
    }
    if (!indexed_paths.empty()) {
      if (use_quantization) {
        if (!sq_quantizer->is_trained() || !pq_quantizer->is_trained()) {
          train_quantizers(all_embeddings,
                           std::visit(
                               [](const auto &embedder_ptr) {
                                 return embedder_ptr->getDimension();
                               },
                               embedder_));
        }
        if (!faiss_index_quantized) {
          faiss_index_quantized =
              std::make_unique<faiss::IndexFlatL2>(std::visit(
                  [](const auto &embedder_ptr) {
                    return embedder_ptr->getDimension();
                  },
                  embedder_));
        } else {
          faiss_index_quantized->reset();
        }
        std::vector<uint8_t> pq_codes;
        for (const auto &path : indexed_paths) {
          const auto &file_info = virtual_files[path];
          if (!file_info.pq_codes.empty()) {
            pq_codes.insert(pq_codes.end(), file_info.pq_codes.begin(),
                            file_info.pq_codes.end());
          }
        }
        faiss_index_quantized->add(
            indexed_paths.size(),
            reinterpret_cast<const float *>(pq_codes.data()));
      } else {
        if (!faiss_index) {
          faiss_index = std::make_unique<faiss::IndexFlatL2>(std::visit(
              [](const auto &embedder_ptr) {
                return embedder_ptr->getDimension();
              },
              embedder_));
        } else {
          faiss_index->reset();
        }
        faiss_index->add(indexed_paths.size(), all_embeddings.data());
      }
      spdlog::info("Index rebuilt with {} files", indexed_paths.size());
      index_needs_rebuild = false;
    }
  }

  std::vector<std::pair<std::string, float>>
  semantic_search(const std::string &query, int k) {
    std::vector<std::pair<std::string, float>> results;
    if (!std::holds_alternative<std::unique_ptr<embedded::FastTextEmbedder>>(
            embedder_) ||
        (!faiss_index && !faiss_index_quantized)) {
      spdlog::error("Embedder or index not initialized");
      return results;
    }
    rebuild_index();
    if (index_to_path.empty()) {
      spdlog::warn("No files indexed for search");
      return results;
    }
    std::string normalized_query = normalize_text(query);
    std::vector<float> query_embedding = std::visit(
        [&normalized_query](auto &embedder_ptr) {
          return embedder_ptr->getSentenceEmbedding(normalized_query);
        },
        embedder_);
    std::vector<idx_t> I(k);
    std::vector<float> D(k);
    if (use_quantization && faiss_index_quantized) {
      if (pq_quantizer->is_trained()) {
        std::vector<uint8_t> query_codes =
            pq_quantizer->encode(query_embedding);
        pq_quantizer->precompute_query_tables(query_embedding);
        std::vector<std::pair<float, std::string>> scored_results;
        for (const auto &[idx, path] : index_to_path) {
          const auto &file_info = virtual_files[path];
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
      faiss_index->search(1, query_embedding.data(), k, D.data(), I.data());
      for (int i = 0; i < k; ++i) {
        if (I[i] >= 0 && index_to_path.find(I[i]) != index_to_path.end()) {
          results.push_back({index_to_path[I[i]], D[i]});
        }
      }
    }
    return results;
  }

  template <typename EmbeddedModel>
  bool initialize(const std::string &model_path,
                  bool use_quantization = false) {
    try {
      if constexpr (std::is_same_v<EmbeddedModel, embedded::FastTextEmbedder>) {
        auto embedder = std::make_unique<embedded::FastTextEmbedder>();
        embedder->loadModel(model_path);
        embedder_ = std::move(embedder);
      }

      this->use_quantization = use_quantization;

      int dimension = std::visit(
          [](const auto &embedder_ptr) { return embedder_ptr->getDimension(); },
          embedder_);

      if (use_quantization) {
        sq_quantizer = std::make_unique<fileinfo::ScalarQuantizer>();
        pq_quantizer = std::make_unique<fileinfo::ProductQuantizer>(8, 256);
        faiss_index_quantized = std::make_unique<faiss::IndexFlatL2>(dimension);
      } else {
        faiss_index = std::make_unique<faiss::IndexFlatL2>(dimension);
      }

      return true;

    } catch (const std::exception &e) {
      spdlog::error("Failed to initialize embedder: {}", e.what());
      return false;
    }
  }

  std::vector<float> get_embedding(const std::string &text) {
    return std::visit(
        [&text](auto &embedder_ptr) {
          return embedder_ptr->getSentenceEmbedding(text);
        },
        embedder_);
  }

  std::string get_embedder_info() const {
    return std::visit(
        [](const auto &embedder_ptr) {
          return fmt::format("Model: {}, Dimension: {}",
                             embedder_ptr->getModelName(),
                             embedder_ptr->getDimension());
        },
        embedder_);
  }

  static VectorFS *getInstance() {
    static VectorFS instance;
    return &instance;
  }

  static inline int getattr_callback(const char *path, struct stat *stbuf,
                                     struct fuse_file_info *fi) {
    return getInstance()->getattr(path, stbuf, fi);
  }

  static inline int readdir_callback(const char *path, void *buf,
                                     fuse_fill_dir_t filler, off_t offset,
                                     struct fuse_file_info *fi,
                                     enum fuse_readdir_flags flags) {
    return getInstance()->readdir(path, buf, filler, offset, fi, flags);
  }

  static inline int open_callback(const char *path, struct fuse_file_info *fi) {
    return getInstance()->open(path, fi);
  }

  static inline int read_callback(const char *path, char *buf, size_t size,
                                  off_t offset, struct fuse_file_info *fi) {
    return getInstance()->read(path, buf, size, offset, fi);
  }

  static inline int write_callback(const char *path, const char *buf,
                                   size_t size, off_t offset,
                                   struct fuse_file_info *fi) {
    return getInstance()->write(path, buf, size, offset, fi);
  }

  static inline int mkdir_callback(const char *path, mode_t mode) {
    return getInstance()->mkdir(path, mode);
  }

  static inline int create_callback(const char *path, mode_t mode,
                                    struct fuse_file_info *fi) {
    return getInstance()->create(path, mode, fi);
  }

  static inline int utimens_callback(const char *path,
                                     const struct timespec tv[2],
                                     struct fuse_file_info *fi) {
    return getInstance()->utimens(path, tv, fi);
  }

  static inline int rmdir_callback(const char *path) {
    return getInstance()->rmdir(path);
  }

  static inline int unlink_callback(const char *path) {
    return getInstance()->unlink(path);
  }

  static inline int getxattr_callback(const char *path, const char *name,
                                      char *value, size_t size) {
    return getInstance()->getxattr(path, name, value, size);
  }

  static inline int setxattr_callback(const char *path, const char *name,
                                      const char *value, size_t size,
                                      int flags) {
    return getInstance()->setxattr(path, name, value, size, flags);
  }

  static inline int listxattr_callback(const char *path, char *list,
                                       size_t size) {
    return getInstance()->listxattr(path, list, size);
  }

  static struct fuse_operations &get_operations() {
    static struct fuse_operations ops = {
        .getattr = getattr_callback,
        .readdir = readdir_callback,
        .open = open_callback,
        .read = read_callback,
        .write = write_callback,
        .mkdir = mkdir_callback,
        .create = create_callback,
        .utimens = utimens_callback,
        .rmdir = rmdir_callback,
        .unlink = unlink_callback,
        .getxattr = getxattr_callback,
        .setxattr = setxattr_callback,
        .listxattr = listxattr_callback,
    };
    return ops;
  }

  std::string generate_markov_test_result();

  void test_semantic_search();
};

} // namespace vfs::vectorfs

#endif // VECTORFS_HPP