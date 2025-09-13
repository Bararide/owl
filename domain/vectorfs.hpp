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

  void test_markov_chains() {
    spdlog::info("=== Testing Markov Chains ===");

    std::vector<std::string> test_files = {
        "/code/main.cpp", "/code/utils.h", "/docs/readme.txt",
        "/config/settings.json", "/tests/test1.py"};

    std::vector<std::string> test_contents = {
        "#include <iostream>\nusing namespace std;",
        "#pragma once\nvoid helper_function();",
        "Project documentation\nImportant information",
        "{\"debug\": true, \"port\": 8080}",
        "def test_function():\n    assert True"};

    for (size_t i = 0; i < test_files.size(); i++) {
      virtual_files[test_files[i]] = fileinfo::FileInfo(
          S_IFREG | 0644, 0, test_contents[i], getuid(), getgid(),
          time(nullptr), time(nullptr), time(nullptr));
      update_embedding(test_files[i]);
    }

    std::vector<std::vector<std::string>> test_sequences = {
        {"/code/main.cpp", "/code/utils.h", "/tests/test1.py"},
        {"/docs/readme.txt", "/code/main.cpp", "/config/settings.json"},
        {"/config/settings.json", "/code/utils.h", "/docs/readme.txt"}};

    for (const auto &seq : test_sequences) {
      hmm_model->add_sequence(seq);
      for (const auto &file : seq) {
        record_file_access(file, "test");
      }
    }

    update_models();

    spdlog::info("Markov chains test completed");
  }

  void update_semantic_relationships() {
    for (const auto &[path1, file1] : virtual_files) {
      if (!file1.embedding_updated || file1.embedding.empty())
        continue;

      for (const auto &[path2, file2] : virtual_files) {
        if (path1 == path2 || !file2.embedding_updated ||
            file2.embedding.empty())
          continue;

        // Вычисляем семантическое сходство
        double similarity =
            calculate_cosine_similarity(file1.embedding, file2.embedding);

        if (similarity > 0.3) { // Порог для создания связи
          semantic_graph->add_edge(path1, path2, similarity);
        }
      }
    }

    spdlog::info("Updated semantic relationships in graph");
  }

  void record_file_access(const std::string &file_path,
                          const std::string &operation = "read") {
    semantic_graph->record_access(file_path, operation);

    // Добавляем в последовательность для HMM
    recent_queries.push_back(file_path);
    if (recent_queries.size() > 50) {
      recent_queries.erase(recent_queries.begin(), recent_queries.begin() + 10);
    }

    // Обучаем HMM на последних последовательностях
    if (recent_queries.size() >= 10) {
      std::vector<std::string> sequence(recent_queries.end() - 10,
                                        recent_queries.end());
      hmm_model->add_sequence(sequence);
    }

    // Периодически переобучаем модели
    auto now = std::chrono::steady_clock::now();
    auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(
                                 now - last_ranking_update)
                                 .count();

    if (time_since_update > 5) { // Каждые 5 минут
      update_models();
      last_ranking_update = now;
    }
  }

  std::vector<std::pair<std::string, float>>
  enhanced_semantic_search(const std::string &query, int k) {
    // Базовый семантический поиск
    auto base_results = semantic_search(query, k * 2);

    // Применяем Random Walk ранжирование
    auto ranking = semantic_graph->random_walk_ranking();

    // Комбинируем результаты
    std::map<std::string, double> combined_scores;

    for (const auto &[file_path, sem_score] : base_results) {
      combined_scores[file_path] = sem_score;
    }

    // Добавляем PageRank веса
    for (const auto &[file_path, pagerank_score] : ranking) {
      if (combined_scores.find(file_path) != combined_scores.end()) {
        combined_scores[file_path] *= (1.0 + pagerank_score);
      }
    }

    // Сортируем и возвращаем
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

          // Добавляем категорию от HMM
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

  int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/.search") == 0) {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      return 0;
    }

    if (strcmp(path, "/.markov") == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 2048;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      return 0;
    }

    if (strncmp(path, "/.search/", 9) == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 1024;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      return 0;
    }

    if (strcmp(path, "/.reindex") == 0 || strcmp(path, "/.embeddings") == 0) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 1024;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      return 0;
    }

    if (virtual_dirs.count(path)) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      time_t now = time(nullptr);
      stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = now;
      return 0;
    }

    auto it = virtual_files.find(path);
    if (it != virtual_files.end()) {
      const auto &fi = it->second;
      stbuf->st_mode = fi.mode;
      stbuf->st_nlink = 1;
      stbuf->st_size = fi.size;
      stbuf->st_uid = fi.uid;
      stbuf->st_gid = fi.gid;
      stbuf->st_atime = fi.access_time;
      stbuf->st_mtime = fi.modification_time;
      stbuf->st_ctime = fi.modification_time;
      return 0;
    }
    return -ENOENT;
  }

  int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
              struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") == 0) {
      filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, ".reindex", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, ".embeddings", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, ".markov", nullptr, 0, FUSE_FILL_DIR_PLUS);

      for (const auto &dir : virtual_dirs) {
        if (dir != "/") {
          filler(buf, dir.c_str() + 1, nullptr, 0, FUSE_FILL_DIR_PLUS);
        }
      }
      for (const auto &file : virtual_files) {
        filler(buf, file.first.c_str() + 1, nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    } else if (strcmp(path, "/.search") == 0) {
      filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
    } else {
      return -ENOENT;
    }
    return 0;
  }

  int rmdir(const char *path) {
    if (virtual_dirs.count(path) == 0) {
      return -ENOENT;
    }

    std::string dir = path;
    if (dir == "/")
      dir = "";

    for (auto it = virtual_files.begin(); it != virtual_files.end();) {
      if (it->first.find(dir) == 0) {
        it = virtual_files.erase(it);
        index_needs_rebuild = true;
      } else {
        ++it;
      }
    }

    virtual_dirs.erase(path);
    return 0;
  }

  int mkdir(const char *path, mode_t mode) {
    if (virtual_dirs.count(path) > 0 || virtual_files.count(path) > 0) {
      return -EEXIST;
    }

    std::string parent_dir = path;
    size_t last_slash = parent_dir.find_last_of('/');
    if (last_slash != std::string::npos) {
      parent_dir = parent_dir.substr(0, last_slash);
      if (parent_dir.empty()) {
        parent_dir = "/";
      }
      if (virtual_dirs.count(parent_dir) == 0) {
        return -ENOENT;
      }
    }

    virtual_dirs.insert(path);
    return 0;
  }

  int open(const char *path, struct fuse_file_info *fi) {
    if (strncmp(path, "/.search/", 9) == 0) {
      return 0;
    }

    if (strcmp(path, "/.reindex") == 0 || strcmp(path, "/.embeddings") == 0) {
      return 0;
    }

    if (virtual_files.count(path) == 0) {
      return -ENOENT;
    }

    return 0;
  }

  int read(const char *path, char *buf, size_t size, off_t offset,
           struct fuse_file_info *fi) {
    record_file_access(path, "read");

    if (strncmp(path, "/.search/", 9) == 0) {
      auto start = std::chrono::high_resolution_clock::now();
      std::string query = path + 9;

      query = url_decode(query);
      std::replace(query.begin(), query.end(), '_', ' ');

      std::string content = generate_enhanced_search_result(query);

      if (offset >= content.size()) {
        return 0;
      }
      size_t len = std::min(content.size() - offset, size);
      memcpy(buf, content.c_str() + offset, len);

      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();
      spdlog::info("Enhanced search for '{}' took {} ms", query, duration);
      return len;
    }

    if (strcmp(path, "/.reindex") == 0) {
      std::stringstream ss;
      ss << "Forcing reindex...\n";
      index_needs_rebuild = true;
      rebuild_index();
      ss << "Reindex completed!\n";
      ss << "Indexed files: " << index_to_path.size() << "\n";

      std::string content = ss.str();
      if (offset >= content.size()) {
        return 0;
      }
      size_t len = std::min(content.size() - offset, size);
      memcpy(buf, content.c_str() + offset, len);
      return len;
    }

    if (strcmp(path, "/.markov") == 0) {
      test_markov_chains();
      std::string content = generate_markov_test_result();

      if (offset >= content.size()) {
        return 0;
      }
      size_t len = std::min(content.size() - offset, size);
      memcpy(buf, content.c_str() + offset, len);
      return len;
    }

    if (strcmp(path, "/.embeddings") == 0) {
      std::stringstream ss;
      ss << "Embeddings report:\n";
      ss << "Total files: " << virtual_files.size() << "\n";
      ss << "Files with embeddings: ";

      int count = 0;
      for (const auto &[file_path, file_info] : virtual_files) {
        if (file_info.embedding_updated && !file_info.embedding.empty()) {
          count++;
          ss << "\n--- " << file_path << " ---\n";
          ss << "Content: "
             << (file_info.content.size() > 50
                     ? file_info.content.substr(0, 50) + "..."
                     : file_info.content)
             << "\n";
          ss << "Embedding size: " << file_info.embedding.size() << "\n";
          ss << "First 5 values: ";
          for (int i = 0; i < std::min(5, (int)file_info.embedding.size());
               ++i) {
            ss << file_info.embedding[i] << " ";
          }
          ss << "\n";
        }
      }
      ss << "Total with embeddings: " << count << "\n";

      std::string content = ss.str();
      if (offset >= content.size()) {
        return 0;
      }
      size_t len = std::min(content.size() - offset, size);
      memcpy(buf, content.c_str() + offset, len);
      return len;
    }

    auto it = virtual_files.find(path);
    if (it == virtual_files.end()) {
      return -ENOENT;
    }

    const std::string &content = it->second.content;
    if (offset >= content.size()) {
      return 0;
    }

    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  int create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (virtual_files.count(path) > 0 || virtual_dirs.count(path) > 0) {
      spdlog::error("File already exists: {}", path);
      return -EEXIST;
    }

    std::string parent_dir = path;
    size_t last_slash = parent_dir.find_last_of('/');
    if (last_slash != std::string::npos) {
      parent_dir = parent_dir.substr(0, last_slash);
      if (parent_dir.empty()) {
        parent_dir = "/";
      }
      if (virtual_dirs.count(parent_dir) == 0) {
        return -ENOENT;
      }
    }

    time_t now = time(nullptr);
    virtual_files[path] = std::move(fileinfo::FileInfo(
        S_IFREG | (mode & 07777), 0, "", getuid(), getgid(), now, now, now));

    return 0;
  }

  int utimens(const char *path, const struct timespec tv[2],
              struct fuse_file_info *fi) {
    auto it = virtual_files.find(path);
    if (it == virtual_files.end()) {
      return -ENOENT;
    }

    it->second.access_time = tv[0].tv_sec;
    it->second.modification_time = tv[1].tv_sec;

    return 0;
  }

  int write(const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    auto it = virtual_files.find(path);
    if (it == virtual_files.end()) {
      return -ENOENT;
    }

    if ((it->second.mode & S_IWUSR) == 0) {
      return -EACCES;
    }

    if (offset + size > it->second.content.size()) {
      it->second.content.resize(offset + size);
    }

    memcpy(&it->second.content[offset], buf, size);

    it->second.size = it->second.content.size();
    it->second.modification_time = time(nullptr);
    it->second.access_time = it->second.modification_time;

    update_embedding(path);

    return size;
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

  int unlink(const char *path) {
    if (virtual_files.erase(path) == 0) {
      return -ENOENT;
    }
    index_needs_rebuild = true;
    return 0;
  }

  int setxattr(const char *path, const char *name, const char *value,
               size_t size, int flags) {
    return -ENOTSUP;
  }

  int getxattr(const char *path, const char *name, char *value, size_t size) {
    auto it = virtual_files.find(path);
    if (it == virtual_files.end())
      return -ENOENT;

    const auto &file_info = it->second;
    std::string attr_value;

    if (strcmp(name, "user.embedding.size") == 0) {
      attr_value = std::to_string(file_info.embedding.size());
    } else if (strcmp(name, "user.embedding.updated") == 0) {
      attr_value = file_info.embedding_updated ? "true" : "false";
    } else if (strcmp(name, "user.content.size") == 0) {
      attr_value = std::to_string(file_info.content.size());
    } else {
      return -ENODATA;
    }

    if (size == 0)
      return attr_value.size();
    if (size < attr_value.size())
      return -ERANGE;

    memcpy(value, attr_value.c_str(), attr_value.size());
    return attr_value.size();
  }

  int listxattr(const char *path, char *list, size_t size) {
    const char *attrs[] = {"user.embedding.size", "user.embedding.updated",
                           "user.content.size", nullptr};

    size_t total_size = 0;
    for (int i = 0; attrs[i]; i++) {
      total_size += strlen(attrs[i]) + 1;
    }

    if (size == 0)
      return total_size;
    if (size < total_size)
      return -ERANGE;

    char *ptr = list;
    for (int i = 0; attrs[i]; i++) {
      strcpy(ptr, attrs[i]);
      ptr += strlen(attrs[i]) + 1;
    }

    return total_size;
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

  std::string generate_markov_test_result() {
    std::stringstream ss;
    ss << "=== Markov Chain Test Results ===\n\n";

    ss << "Semantic Graph Analysis:\n";
    ss << "Nodes: " << semantic_graph->get_node_count() << "\n";
    ss << "Edges: " << semantic_graph->get_edge_count() << "\n";

    auto hubs = semantic_graph->get_semantic_hubs(5);
    if (!hubs.empty()) {
      ss << "Top semantic hubs:\n";
      for (const auto &hub : hubs) {
        ss << "  ⭐ " << hub << "\n";
      }
    }
    ss << "\n";

    ss << "Hidden Markov Model Analysis:\n";
    ss << "Trained states: " << hmm_model->get_state_count() << "\n";
    ss << "Observation sequences: " << hmm_model->get_sequence_count() << "\n";

    auto predictions = predict_next_files();
    if (!predictions.empty()) {
      ss << "Current predictions:\n";
      for (const auto &pred : predictions) {
        ss << "  ↗ " << pred << "\n";
      }
    }
    ss << "\n";

    ss << "Recommendation Test:\n";
    int test_count = 0;
    for (const auto &[path, _] : virtual_files) {
      if (test_count >= 3)
        break;

      auto recs = semantic_graph->get_recommendations(path, 3);
      if (!recs.empty()) {
        ss << "For " << path << ":\n";
        for (const auto &rec : recs) {
          ss << "  → " << rec << "\n";
        }
        ss << "\n";
        test_count++;
      }
    }

    ss << "Access Patterns:\n";
    ss << "Recent queries: " << recent_queries.size() << "\n";
    if (!recent_queries.empty()) {
      ss << "Last 5 queries:\n";
      for (int i = std::max(0, (int)recent_queries.size() - 5);
           i < recent_queries.size(); i++) {
        ss << "  " << recent_queries[i] << "\n";
      }
    }

    return ss.str();
  }

  void test_semantic_search() {
    if (!std::holds_alternative<std::unique_ptr<embedded::FastTextEmbedder>>(
            embedder_) ||
        !faiss_index) {
      spdlog::error("Embedder or index not initialized for testing");
      return;
    }

    spdlog::info("=== Enhanced Semantic Search Test with Code Examples ===");

    std::vector<std::pair<std::string, std::string>> test_files = {
        // Python функции и основы
        {"/python/functions_basic.py",
         "def greet(name):\n    return f\"Hello, {name}!\"\n\n# Функции в "
         "Python объявляются через def\n# Могут возвращать значения через "
         "return\n# Поддерживают аргументы по умолчанию\n\ndef "
         "calculate_area(radius, pi=3.14159):\n    return pi * radius * "
         "radius"},

        {"/python/functions_advanced.py",
         "# Лямбда-функции в Python\nsquare = lambda x: x * x\n\n# Функции "
         "высшего порядка\ndef apply_func(func, value):\n    return "
         "func(value)\n\n# Декораторы функций\ndef debug_decorator(func):\n    "
         "def wrapper(*args, **kwargs):\n        print(f\"Calling "
         "{func.__name__}\")\n        return func(*args, **kwargs)\n    return "
         "wrapper"},

        {"/python/oop_basics.py",
         "class Car:\n    def __init__(self, brand, model):\n        "
         "self.brand = brand\n        self.model = model\n    \n    def "
         "display_info(self):\n        return f\"{self.brand} "
         "{self.model}\"\n\n# Наследование в Python\nclass ElectricCar(Car):\n "
         "   def __init__(self, brand, model, battery_size):\n        "
         "super().__init__(brand, model)\n        self.battery_size = "
         "battery_size"},

        // C++ функции и основы
        {"/cpp/functions_basic.cpp",
         "// Функции в C++ объявляются с указанием типа возвращаемого "
         "значения\nint add(int a, int b) {\n    return a + b;\n}\n\n// "
         "Функции могут принимать параметры по ссылке\nvoid swap(int &a, int "
         "&b) {\n    int temp = a;\n    a = b;\n    b = temp;\n}"},

        {"/cpp/functions_advanced.cpp",
         "// Шаблонные функции в C++\ntemplate<typename T>\nT max(T a, T b) "
         "{\n    return (a > b) ? a : b;\n}\n\n// Рекурсивные функции\nint "
         "factorial(int n) {\n    if (n <= 1) return 1;\n    return n * "
         "factorial(n - 1);\n}\n\n// Указатели на функции\nint operate(int a, "
         "int b, int (*func)(int, int)) {\n    return func(a, b);\n}"},

        {"/cpp/oop_basics.cpp",
         "class Car {\nprivate:\n    std::string brand;\n    std::string "
         "model;\npublic:\n    Car(std::string b, std::string m) : brand(b), "
         "model(m) {}\n    \n    std::string displayInfo() {\n        return "
         "brand + \" \" + model;\n    }\n};\n\n// Наследование в C++\nclass "
         "ElectricCar : public Car {\nprivate:\n    int "
         "battery_size;\npublic:\n    ElectricCar(std::string b, std::string "
         "m, int bs) \n        : Car(b, m), battery_size(bs) {}\n};"},

        // Сравнительные примеры Python vs C++
        {"/comparison/functions_python_vs_cpp.txt",
         "Сравнение функций в Python и C++:\n\nPython: def "
         "function_name(args):\nC++: return_type function_name(parameters) "
         "{}\n\nPython: динамическая типизация\nC++: статическая "
         "типизация\n\nPython: поддержка lambda\nC++: поддержка lambda с "
         "C++11\n\nPython: декораторы функций\nC++: шаблоны и функторы"},

        {"/comparison/oop_python_vs_cpp.txt",
         "Сравнение ООП в Python и C++:\n\nНаследование:\nPython: class "
         "Child(Parent):\nC++: class Child : public "
         "Parent\n\nИнкапсуляция:\nPython: соглашения (_protected, "
         "__private)\nC++: модификаторы private, protected, "
         "public\n\nПолиморфизм:\nPython: duck typing\nC++: виртуальные "
         "функции\n\nКонструкторы:\nPython: def __init__(self):\nC++: "
         "ClassName() : initialization_list {}"},

        // Алгоритмы на обоих языках
        {"/algorithms/sort_python.py",
         "def bubble_sort(arr):\n    n = len(arr)\n    for i in range(n):\n    "
         "    for j in range(0, n-i-1):\n            if arr[j] > arr[j+1]:\n   "
         "             arr[j], arr[j+1] = arr[j+1], arr[j]\n\n# Быстрая "
         "сортировка на Python\ndef quick_sort(arr):\n    if len(arr) <= 1:\n  "
         "      return arr\n    pivot = arr[len(arr)//2]\n    left = [x for x "
         "in arr if x < pivot]\n    middle = [x for x in arr if x == pivot]\n  "
         "  right = [x for x in arr if x > pivot]\n    return quick_sort(left) "
         "+ middle + quick_sort(right)"},

        {"/algorithms/sort_cpp.cpp",
         "// Пузырьковая сортировка на C++\nvoid bubbleSort(int arr[], int n) "
         "{\n    for (int i = 0; i < n-1; i++) {\n        for (int j = 0; j < "
         "n-i-1; j++) {\n            if (arr[j] > arr[j+1]) {\n                "
         "std::swap(arr[j], arr[j+1]);\n            }\n        }\n    "
         "}\n}\n\n// Быстрая сортировка на C++\nvoid quickSort(int arr[], int "
         "low, int high) {\n    if (low < high) {\n        int pi = "
         "partition(arr, low, high);\n        quickSort(arr, low, pi - 1);\n   "
         "     quickSort(arr, pi + 1, high);\n    }\n}"},

        // Структуры данных
        {"/data_structures/linked_list_python.py",
         "class Node:\n    def __init__(self, data):\n        self.data = "
         "data\n        self.next = None\n\nclass LinkedList:\n    def "
         "__init__(self):\n        self.head = None\n    \n    def "
         "append(self, data):\n        new_node = Node(data)\n        if not "
         "self.head:\n            self.head = new_node\n            return\n   "
         "     last = self.head\n        while last.next:\n            last = "
         "last.next\n        last.next = new_node"},

        {"/data_structures/linked_list_cpp.cpp",
         "struct Node {\n    int data;\n    Node* next;\n    Node(int d) : "
         "data(d), next(nullptr) {}\n};\n\nclass LinkedList {\nprivate:\n    "
         "Node* head;\npublic:\n    LinkedList() : head(nullptr) {}\n    \n    "
         "void append(int data) {\n        Node* new_node = new Node(data);\n  "
         "      if (!head) {\n            head = new_node;\n            "
         "return;\n        }\n        Node* last = head;\n        while "
         "(last->next) {\n            last = last->next;\n        }\n        "
         "last->next = new_node;\n    }\n};"},

        // Многопоточность
        {"/concurrency/threads_python.py",
         "import threading\nimport time\n\ndef print_numbers():\n    for i in "
         "range(5):\n        time.sleep(1)\n        print(i)\n\n# Создание "
         "потоков в Python\nthread1 = "
         "threading.Thread(target=print_numbers)\nthread2 = "
         "threading.Thread(target=print_numbers)\nthread1.start()\nthread2."
         "start()\nthread1.join()\nthread2.join()"},

        {"/concurrency/threads_cpp.cpp",
         "#include <thread>\n#include <iostream>\n#include <chrono>\n\nvoid "
         "printNumbers() {\n    for (int i = 0; i < 5; ++i) {\n        "
         "std::this_thread::sleep_for(std::chrono::seconds(1));\n        "
         "std::cout << i << std::endl;\n    }\n}\n\n// Создание потоков в "
         "C++\nint main() {\n    std::thread thread1(printNumbers);\n    "
         "std::thread thread2(printNumbers);\n    thread1.join();\n    "
         "thread2.join();\n    return 0;\n}"},

        // Веб и сети
        {"/web/http_server_python.py",
         "from flask import Flask\napp = "
         "Flask(__name__)\n\n@app.route('/')\ndef hello():\n    return \"Hello "
         "World!\"\n\nif __name__ == '__main__':\n    app.run()\n\n# Простой "
         "HTTP сервер на Python Flask"},

        {"/web/http_server_cpp.cpp",
         "#include <cpprest/http_listener.h>\n#include "
         "<cpprest/json.h>\n\nusing namespace web;\nusing namespace "
         "http;\nusing namespace http::experimental::listener;\n\nvoid "
         "handle_get(http_request request) {\n    "
         "request.reply(status_codes::OK, \"Hello World!\");\n}\n\n// Простой "
         "HTTP сервер на C++ REST SDK"},

        // Машинное обучение
        {"/ml/linear_regression_python.py",
         "import numpy as np\nfrom sklearn.linear_model import "
         "LinearRegression\n\n# Линейная регрессия на Python\nX = "
         "np.array([[1], [2], [3], [4], [5]])\ny = np.array([1, 2, 3, 4, "
         "5])\n\nmodel = LinearRegression()\nmodel.fit(X, y)\npredictions = "
         "model.predict([[6], [7]])"},

        {"/ml/neural_network_python.py",
         "import tensorflow as tf\nfrom tensorflow.keras import "
         "Sequential\nfrom tensorflow.keras.layers import Dense\n\n# Нейронная "
         "сеть на Python\nmodel = Sequential([\n    Dense(64, "
         "activation='relu', input_shape=(10,)),\n    Dense(32, "
         "activation='relu'),\n    Dense(1, "
         "activation='sigmoid')\n])\nmodel.compile(optimizer='adam', "
         "loss='binary_crossentropy')"},

        // Системное программирование
        {"/system/file_io_python.py",
         "# Работа с файлами в Python\nwith open('file.txt', 'r') as file:\n   "
         " content = file.read()\n\nwith open('output.txt', 'w') as file:\n    "
         "file.write('Hello World!')\n\n# Чтение бинарных файлов\nwith "
         "open('image.jpg', 'rb') as file:\n    data = file.read()"},

        {"/system/file_io_cpp.cpp",
         "// Работа с файлами в C++\n#include <fstream>\n#include "
         "<string>\n\nstd::ifstream file(\"file.txt\");\nstd::string "
         "content;\nif (file.is_open()) {\n    "
         "content.assign((std::istreambuf_iterator<char>(file)),\n             "
         "      std::istreambuf_iterator<char>());\n    "
         "file.close();\n}\n\nstd::ofstream outfile(\"output.txt\");\noutfile "
         "<< \"Hello World!\";\noutfile.close();"},

        // Базы данных
        {"/database/sqlite_python.py",
         "import sqlite3\n\n# Работа с SQLite в Python\nconn = "
         "sqlite3.connect('example.db')\ncursor = "
         "conn.cursor()\n\ncursor.execute('''CREATE TABLE users\n              "
         " (id INTEGER PRIMARY KEY, name TEXT, age "
         "INTEGER)''')\n\ncursor.execute(\"INSERT INTO users (name, age) "
         "VALUES (?, ?)\", (\"Alice\", 25))\nconn.commit()\nconn.close()"},

        {"/database/mysql_cpp.cpp",
         "// Работа с MySQL в C++\n#include <mysql_driver.h>\n#include "
         "<mysql_connection.h>\n#include "
         "<cppconn/prepared_statement.h>\n\nsql::mysql::MySQL_Driver "
         "*driver;\nsql::Connection *con;\n\ndriver = "
         "sql::mysql::get_mysql_driver_instance();\ncon = "
         "driver->connect(\"tcp://127.0.0.1:3306\", \"user\", "
         "\"password\");\n\nsql::PreparedStatement *pstmt;\npstmt = "
         "con->prepareStatement(\"INSERT INTO users(name, age) VALUES(?, "
         "?)\");\npstmt->setString(1, \"Alice\");\npstmt->setInt(2, "
         "25);\npstmt->execute();\ndelete pstmt;\ndelete con;"}};

    for (const auto &[path, content] : test_files) {
      virtual_files[path] =
          fileinfo::FileInfo(S_IFREG | 0644, 0, content, getuid(), getgid(),
                             time(nullptr), time(nullptr), time(nullptr));
      update_embedding(path);
    }

    std::vector<std::vector<std::string>> access_patterns = {
        {"/python/functions_basic.py", "/python/oop_basics.py",
         "/algorithms/sort_python.py", "/web/http_server_python.py",
         "/ml/linear_regression_python.py"},

        {"/cpp/functions_basic.cpp", "/cpp/oop_basics.cpp",
         "/algorithms/sort_cpp.cpp", "/system/file_io_cpp.cpp",
         "/web/http_server_cpp.cpp"},

        {"/comparison/functions_python_vs_cpp.txt",
         "/web/http_server_python.py", "/web/http_server_cpp.cpp",
         "/database/sqlite_python.py", "/database/mysql_cpp.cpp"},

        {"/ml/linear_regression_python.py", "/ml/neural_network_python.py",
         "/python/functions_advanced.py", "/algorithms/sort_python.py",
         "/data_structures/linked_list_python.py"},

        {"/system/file_io_cpp.cpp", "/concurrency/threads_cpp.cpp",
         "/cpp/functions_advanced.cpp", "/algorithms/sort_cpp.cpp",
         "/data_structures/linked_list_cpp.cpp"}};

    for (const auto &pattern : access_patterns) {
      for (const auto &file : pattern) {
        record_file_access(file, "test_pattern");
      }
      hmm_model->add_sequence(pattern);
    }

    rebuild_index();
    update_models();

    std::vector<std::pair<std::string, std::string>> test_queries = {
        {"программирование", "Общие запросы по программированию"},
        {"нейронные сети", "Специфичные ML запросы"},
        {"SQL база данных", "Запросы по базам данных"},
        {"веб приложение", "Веб-разработка"},
        {"Linux система", "Системное администрирование"},
        {"обработка данных", "Межкатегорийные запросы"}};

    spdlog::info("Total indexed files: {}", index_to_path.size());
    spdlog::info("HMM states: {}", hmm_model->get_state_count());
    spdlog::info("Semantic graph edges: {}", semantic_graph->get_edge_count());
    spdlog::info("");

    for (const auto &[query, description] : test_queries) {
      spdlog::info("=== Query: '{}' ({}) ===", query, description);

      auto basic_results = semantic_search(query, 5);
      spdlog::info("Basic semantic search results:");
      if (basic_results.empty()) {
        spdlog::warn("  No results found");
      } else {
        for (const auto &[path, score] : basic_results) {
          auto category = hmm_model->classify_file_category(path, {});
          spdlog::info("  📄 {} (score: {:.3f}, category: {})", path, score,
                       category);

          auto it = virtual_files.find(path);
          if (it != virtual_files.end()) {
            spdlog::info("    Content: {}...",
                         it->second.content.substr(0, 40));
          }
        }
      }

      auto enhanced_results = enhanced_semantic_search(query, 3);
      spdlog::info("Enhanced search with Markov chains:");
      if (enhanced_results.empty()) {
        spdlog::warn("  No enhanced results found");
      } else {
        for (const auto &[path, score] : enhanced_results) {
          auto category = hmm_model->classify_file_category(path, {});
          spdlog::info("  🚀 {} (score: {:.3f}, category: {})", path, score,
                       category);
        }
      }

      if (!basic_results.empty()) {
        auto recommendations = get_recommendations_for_query(query);
        if (!recommendations.empty()) {
          spdlog::info("Related recommendations:");
          for (const auto &rec : recommendations) {
            auto category = hmm_model->classify_file_category(rec, {});
            spdlog::info("  → {} (category: {})", rec, category);
          }
        }
      }

      spdlog::info("---");
    }

    spdlog::info("=== Additional Statistics ===");

    auto hubs = semantic_graph->get_semantic_hubs(5);
    if (!hubs.empty()) {
      spdlog::info("Top semantic hubs:");
      for (const auto &hub : hubs) {
        auto category = hmm_model->classify_file_category(hub, {});
        spdlog::info("  ⭐ {} (category: {})", hub, category);
      }
    }

    auto predictions = predict_next_files();
    if (!predictions.empty()) {
      spdlog::info("Predicted next files:");
      for (const auto &pred : predictions) {
        auto category = hmm_model->classify_file_category(pred, {});
        spdlog::info("  🔮 {} (category: {})", pred, category);
      }
    }

    spdlog::info("File categorization summary:");
    std::map<std::string, int> category_counts;
    for (const auto &[path, _] : virtual_files) {
      std::string category = hmm_model->classify_file_category(path, {});
      category_counts[category]++;
    }

    for (const auto &[category, count] : category_counts) {
      spdlog::info("  {}: {} files", category, count);
    }
  }
};

} // namespace vfs::vectorfs

#endif // VECTORFS_HPP