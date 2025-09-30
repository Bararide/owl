#ifndef OWL_FAISS_SERVICE_HPP
#define OWL_FAISS_SERVICE_HPP

#include "file/fileinfo.hpp"
#include "utils/quantization.hpp"
#include <algorithm>
#include <chrono>
#include <faiss/IndexFlat.h>
#include <iomanip>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <vector>

namespace owl::faiss {

class FaissService {
public:
  FaissService(int dimension, bool use_quantization = false);
  ~FaissService() = default;

  FaissService(const FaissService &) = delete;
  FaissService &operator=(const FaissService &) = delete;
  FaissService(FaissService &&) = delete;
  FaissService &operator=(FaissService &&) = delete;

  void add_embedding(const std::vector<float> &embedding,
                     const std::string &path,
                     const fileinfo::FileInfo &file_info);
  std::vector<std::pair<std::string, float>>
  semantic_search(const std::vector<float> &query_embedding, int k = 10);

  void rebuild_index();
  void clear_index();
  size_t size() const;

  void train_quantizers(const std::vector<float> &embeddings, size_t dim);

  std::string get_index_info() const;
  int get_dimension() const { return dimension_; }

  std::map<std::string, fileinfo::FileInfo> &get_virtual_files() {
    return virtual_files_;
  }
  std::map<::faiss::idx_t, std::string> &get_index_to_path() {
    return index_to_path_;
  }

private:
  int dimension_;
  bool use_quantization_;
  bool index_needs_rebuild_;

  std::unique_ptr<::faiss::IndexFlatL2> faiss_index_;
  std::unique_ptr<::faiss::IndexFlatL2> faiss_index_quantized_;

  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;

  std::map<::faiss::idx_t, std::string> index_to_path_;
  std::map<std::string, fileinfo::FileInfo> virtual_files_;
  ::faiss::idx_t next_index_id_;

  void initialize_index();
};

inline FaissService::FaissService(int dimension, bool use_quantization)
    : dimension_(dimension), use_quantization_(use_quantization),
      index_needs_rebuild_(true), next_index_id_(0) {

  if (use_quantization_) {
    sq_quantizer_ = std::make_unique<utils::ScalarQuantizer>();
    pq_quantizer_ = std::make_unique<utils::ProductQuantizer>(8, 256);
  }

  initialize_index();
  spdlog::info("FaissService initialized with dimension: {}, quantization: {}",
               dimension_, use_quantization_);
}

inline void FaissService::initialize_index() {
  if (use_quantization_) {
    faiss_index_quantized_ = std::make_unique<::faiss::IndexFlatL2>(dimension_);
    spdlog::info("Quantized FAISS index initialized");
  } else {
    faiss_index_ = std::make_unique<::faiss::IndexFlatL2>(dimension_);
    spdlog::info("Standard FAISS index initialized");
  }
}

inline void FaissService::add_embedding(const std::vector<float> &embedding,
                                        const std::string &path,
                                        const fileinfo::FileInfo &file_info) {
  if (embedding.size() != dimension_) {
    spdlog::error("Embedding dimension mismatch: expected {}, got {}",
                  dimension_, embedding.size());
    return;
  }

  virtual_files_[path] = file_info;

  virtual_files_[path].embedding = embedding;
  virtual_files_[path].embedding_updated = true;

  if (use_quantization_ && sq_quantizer_ && sq_quantizer_->is_trained()) {
    virtual_files_[path].sq_codes = sq_quantizer_->quantize(embedding);
  }
  if (use_quantization_ && pq_quantizer_ && pq_quantizer_->is_trained()) {
    virtual_files_[path].pq_codes = pq_quantizer_->encode(embedding);
  }

  index_needs_rebuild_ = true;
  spdlog::debug("Added embedding for path: {}", path);
}

inline std::vector<std::pair<std::string, float>>
FaissService::semantic_search(const std::vector<float> &query_embedding,
                              int k) {
  std::vector<std::pair<std::string, float>> results;

  if (!faiss_index_ && !faiss_index_quantized_) {
    spdlog::error("FAISS index not initialized");
    return results;
  }

  rebuild_index();

  if (index_to_path_.empty()) {
    spdlog::warn("No files indexed for search");
    return results;
  }

  std::vector<::faiss::idx_t> I(k);
  std::vector<float> D(k);

  if (use_quantization_ && faiss_index_quantized_) {
    if (pq_quantizer_ && pq_quantizer_->is_trained()) {
      std::vector<uint8_t> query_codes = pq_quantizer_->encode(query_embedding);
      pq_quantizer_->precompute_query_tables(query_embedding);
      std::vector<std::pair<float, std::string>> scored_results;

      for (const auto &[idx, path] : index_to_path_) {
        const auto &file_info = virtual_files_[path];
        if (!file_info.pq_codes.empty()) {
          float dist = pq_quantizer_->asymmetric_distance(file_info.pq_codes);
          scored_results.emplace_back(dist, path);
        }
      }

      std::sort(scored_results.begin(), scored_results.end());
      for (int i = 0; i < std::min(k, (int)scored_results.size()); ++i) {
        results.push_back({scored_results[i].second, scored_results[i].first});
      }
    }
  } else if (faiss_index_) {
    faiss_index_->search(1, query_embedding.data(), k, D.data(), I.data());
    for (int i = 0; i < k; ++i) {
      if (I[i] >= 0 && index_to_path_.find(I[i]) != index_to_path_.end()) {
        results.push_back({index_to_path_[I[i]], D[i]});
      }
    }
  }

  return results;
}

inline void FaissService::rebuild_index() {
  if (!index_needs_rebuild_)
    return;

  spdlog::info("Rebuilding vector index (quantization: {})", use_quantization_);
  index_to_path_.clear();

  std::vector<float> all_embeddings;
  std::vector<std::string> indexed_paths;
  ::faiss::idx_t idx = 0;

  for (const auto &[path, file_info] : virtual_files_) {
    if (file_info.embedding_updated && !file_info.embedding.empty()) {
      all_embeddings.insert(all_embeddings.end(), file_info.embedding.begin(),
                            file_info.embedding.end());
      index_to_path_[idx] = path;
      indexed_paths.push_back(path);
      idx++;
    }
  }

  if (!indexed_paths.empty()) {
    if (use_quantization_) {
      if ((sq_quantizer_ && !sq_quantizer_->is_trained()) ||
          (pq_quantizer_ && !pq_quantizer_->is_trained())) {
        train_quantizers(all_embeddings, dimension_);
      }

      if (!faiss_index_quantized_) {
        faiss_index_quantized_ =
            std::make_unique<::faiss::IndexFlatL2>(dimension_);
      } else {
        faiss_index_quantized_->reset();
      }

      std::vector<uint8_t> pq_codes;
      for (const auto &path : indexed_paths) {
        const auto &file_info = virtual_files_[path];
        if (!file_info.pq_codes.empty()) {
          pq_codes.insert(pq_codes.end(), file_info.pq_codes.begin(),
                          file_info.pq_codes.end());
        }
      }

      faiss_index_quantized_->add(
          indexed_paths.size(),
          reinterpret_cast<const float *>(pq_codes.data()));
    } else {
      if (!faiss_index_) {
        faiss_index_ = std::make_unique<::faiss::IndexFlatL2>(dimension_);
      } else {
        faiss_index_->reset();
      }

      faiss_index_->add(indexed_paths.size(), all_embeddings.data());
    }

    spdlog::info("Index rebuilt with {} files", indexed_paths.size());
    index_needs_rebuild_ = false;
  }
}

inline void FaissService::clear_index() {
  virtual_files_.clear();
  index_to_path_.clear();
  next_index_id_ = 0;
  index_needs_rebuild_ = true;
  initialize_index();
  spdlog::info("FAISS index cleared");
}

inline size_t FaissService::size() const {
  if (use_quantization_ && faiss_index_quantized_) {
    return faiss_index_quantized_->ntotal;
  } else if (faiss_index_) {
    return faiss_index_->ntotal;
  }
  return 0;
}

inline void FaissService::train_quantizers(const std::vector<float> &embeddings,
                                           size_t dim) {
  if (embeddings.empty())
    return;

  std::vector<std::vector<float>> training_data;
  size_t num_vectors = embeddings.size() / dim;

  for (size_t i = 0; i < num_vectors; i++) {
    training_data.emplace_back(embeddings.begin() + i * dim,
                               embeddings.begin() + (i + 1) * dim);
  }

  if (sq_quantizer_) {
    sq_quantizer_->train(training_data, dim);
  }

  if (pq_quantizer_) {
    pq_quantizer_->train(training_data, dim);
  }

  spdlog::info("Quantizers trained with {} vectors", num_vectors);
}

inline std::string FaissService::get_index_info() const {
  return fmt::format("FaissService[dim={}, quantized={}, size={}, files={}]",
                     dimension_, use_quantization_, size(),
                     virtual_files_.size());
}

} // namespace owl::faiss

#endif // OWL_FAISS_SERVICE_HPP