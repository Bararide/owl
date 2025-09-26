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
#include <variant>
#include <vector>

#include <faiss/IndexFlat.h>
#include <fasttext.h>
#include <infrastructure/measure.hpp>
#include <spdlog/spdlog.h>

#include "algorithms/compression/compression.hpp"
#include "algorithms/compression/compression_base.hpp"
#include "embedded/embedded_base.hpp"
#include "embedded/embedded_fasttext.hpp"
#include "file/fileinfo.hpp"
#include "markov.hpp"
#include "shared_memory/shared_memory.hpp"
#include "utils/quantization.hpp"

namespace owl::vectorfs {

using EmbedderVariant =
    std::variant<std::unique_ptr<embedded::FastTextEmbedder>>;
using idx_t = faiss::idx_t;

class VectorFS {
public:
  static VectorFS *getInstance() {
    static VectorFS instance;
    return &instance;
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

  template <typename EmbeddedModel, typename Compressor>
  bool initialize(const std::string &model_path,
                  bool use_quantization = false) {
    try {
      initialize_shared_memory();
      initialize_embedder<EmbeddedModel>(model_path, use_quantization);
      initialize_compressor<Compressor>();
      initialize_markov_chain();

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to initialize embedder: {}", e.what());
      return false;
    }
  }

  void initialize_markov_chain() {
    semantic_graph_ = std::make_unique<markov::SemanticGraph>();
    hmm_model_ = std::make_unique<markov::HiddenMarkovModel>();
  }

  void updateFromSharedMemory();

  std::vector<float> get_embedding(const std::string &text);
  std::string get_embedder_info() const;

  std::map<std::string, fileinfo::FileInfo> &get_virtual_files() {
    return virtual_files_;
  }

  void rebuild_index();
  std::vector<std::pair<std::string, float>>
  semantic_search(const std::string &query, int k);
  void update_semantic_relationships();
  void record_file_access(const std::string &file_path,
                          const std::string &operation);

  std::string generate_markov_test_result();
  void test_markov_chains();
  void test_semantic_search();

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

  ~VectorFS() = default;

private:
  VectorFS() = default;

  void initialize_shared_memory() {
    shm_manager_ = std::make_unique<shared::SharedMemoryManager>();
    if (!shm_manager_->initialize()) {
      spdlog::warn("Failed to initialize shared memory");
    }
  }

  template <typename EmbeddedModel>
  void initialize_embedder(const std::string &model_path,
                           bool use_quantization = false) {
    if constexpr (std::is_same_v<EmbeddedModel, embedded::FastTextEmbedder>) {
      auto embedder = std::make_unique<embedded::FastTextEmbedder>();
      embedder->loadModel(std::move(model_path));
      embedder_ = std::move(embedder);
    }

    this->use_quantization_ = use_quantization;

    int dimension = std::visit(
        [](const auto &embedder_ptr) { return embedder_ptr->getDimension(); },
        embedder_);

    if (use_quantization) {
      sq_quantizer_ = std::make_unique<utils::ScalarQuantizer>();
      pq_quantizer_ = std::make_unique<utils::ProductQuantizer>(8, 256);
      faiss_index_quantized_ = std::make_unique<faiss::IndexFlatL2>(dimension);
    } else {
      faiss_index_ = std::make_unique<faiss::IndexFlatL2>(dimension);
    }
  }

  template <typename Compressor> void initialize_compressor() {
    compressor_ = std::make_unique<compression::Compressor>();
  }

  std::string normalize_text(const std::string &text);
  double calculate_cosine_similarity(const std::vector<float> &a,
                                     const std::vector<float> &b);
  void update_models();
  std::vector<std::string>
  get_recommendations_for_query(const std::string &query);
  void update_predictive_cache();
  std::string url_decode(const std::string &str);
  void update_embedding(const std::string &path);
  std::string generate_search_result(const std::string &query);
  std::vector<std::pair<std::string, float>>
  enhanced_semantic_search(const std::string &query, int k);
  std::string generate_enhanced_search_result(const std::string &query);
  void train_quantizers(const std::vector<float> &embeddings, size_t dim);
  std::vector<std::string> predict_next_files();

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

private:
  std::map<std::string, fileinfo::FileInfo> virtual_files_;
  std::set<std::string> virtual_dirs_;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index_;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index_quantized_;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;
  std::unique_ptr<shared::SharedMemoryManager> shm_manager_;
  std::unique_ptr<compression::Compressor> compressor_;
  std::map<idx_t, std::string> index_to_path_;
  std::map<std::string, std::string> search_results_cache_;

  std::unique_ptr<markov::SemanticGraph> semantic_graph_;
  std::unique_ptr<markov::HiddenMarkovModel> hmm_model_;
  std::vector<std::string> recent_queries_;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update_;

  std::map<std::string, std::vector<std::string>> predictive_cache_;

  EmbedderVariant embedder_;
  std::string model_type_;

  bool index_needs_rebuild_;
  bool use_quantization_;
};

} // namespace owl::vectorfs

#endif // VECTORFS_HPP