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
#include <infrastructure/measure.hpp>
#include <infrastructure/result.hpp>
#include <spdlog/spdlog.h>

#include "algorithms/compressor/compressor.hpp"
#include "container_manager.hpp"
#include "file/fileinfo.hpp"
#include "markov.hpp"
#include "shared_memory/shared_memory.hpp"
#include "utils/quantization.hpp"
#include <embedded/embedded_base.hpp>
#include <embedded/emdedded_manager.hpp>
#include <memory/container_builder.hpp>

namespace owl::vectorfs {

using CompressorVariant =
    std::variant<std::unique_ptr<compression::Compressor>>;
using idx_t = faiss::idx_t;

class VectorFS {
private:
  std::map<std::string, fileinfo::FileInfo> virtual_files;
  std::set<std::string> virtual_dirs;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index;
  std::unique_ptr<faiss::IndexFlatL2> faiss_index_quantized;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer;
  std::unique_ptr<owl::shared::SharedMemoryManager> shm_manager;
  bool index_needs_rebuild;
  bool use_quantization;
  std::map<idx_t, std::string> index_to_path;
  std::map<std::string, std::string> search_results_cache;

  owl::vectorfs::ContainerManager &container_manager_;

  std::unique_ptr<markov::SemanticGraph> semantic_graph;
  std::unique_ptr<markov::HiddenMarkovModel> hmm_model;
  std::vector<std::string> recent_queries;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update;

  std::map<std::string, std::vector<std::string>> predictive_cache;

  EmbedderManager<> embedder_;
  std::string model_type_;

  CompressorVariant compressor_;

  void test_container();
  void initialize_container_paths();
  std::shared_ptr<IKnowledgeContainer>
  get_container_for_path(const std::string &path);
  std::string generate_container_listing();
  std::string generate_container_content(const std::string &container_id);
  std::string handle_container_search(const std::string &container_id,
                                      const std::string &query);

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

  std::vector<uint8_t> compress_data(const std::vector<uint8_t> &data) {
    return std::visit(
        [&](const auto &compressor) -> std::vector<uint8_t> {
          auto result = compressor->compress_impl(data);
          return result.is_ok() ? result.unwrap() : data;
        },
        compressor_);
  }

  std::vector<uint8_t>
  decompress_data(const std::vector<uint8_t> &compressed_data) {
    return std::visit(
        [&](const auto &compressor) -> std::vector<uint8_t> {
          auto result = compressor->decompress_impl(compressed_data);
          return result.is_ok() ? result.unwrap() : compressed_data;
        },
        compressor_);
  }

  std::vector<uint8_t> get_compressed_data_from_shm(const std::string &path) {
    if (!shm_manager || !shm_manager->initialize())
      return {};
    for (int i = 0; i < shm_manager->getFileCount(); i++) {
      const auto *shared_info = shm_manager->getFile(i);
      if (shared_info && std::string(shared_info->path) == path) {
        std::vector<uint8_t> compressed_data(
            shared_info->content, shared_info->content + shared_info->size);
        return decompress_data(compressed_data);
      }
    }
    return {};
  }

public:
  VectorFS()
      : virtual_dirs({"/"}), index_needs_rebuild(true), use_quantization(false),
        container_manager_(owl::vectorfs::ContainerManager::get_instance()) {
    semantic_graph = std::make_unique<markov::SemanticGraph>();
    hmm_model = std::make_unique<markov::HiddenMarkovModel>();
    last_ranking_update = std::chrono::steady_clock::now();
    hmm_model->add_state("code");
    hmm_model->add_state("document");
    hmm_model->add_state("config");
    hmm_model->add_state("test");
    hmm_model->add_state("misc");
    initialize_container_paths();
    test_container();
  }

  // FUSE операции
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

  std::map<std::string, fileinfo::FileInfo> &get_virtual_files() {
    return virtual_files;
  }

  void updateFromSharedMemory();
  void rebuild_index();
  std::vector<std::pair<std::string, float>>
  semantic_search(const std::string &query, int k);
  void update_semantic_relationships();
  void record_file_access(const std::string &file_path,
                          const std::string &operation);

  template <typename EmbeddedModel,
            typename CompressorType = compression::Compressor>
  bool initialize(const std::string model_path, bool use_quantization = false) {
    try {
      shm_manager = std::make_unique<owl::shared::SharedMemoryManager>();
      if (!shm_manager->initialize())
        spdlog::warn("Failed to initialize shared memory");

      if constexpr (std::is_same_v<EmbeddedModel, embedded::FastTextEmbedder>) {
        embedder_.set(std::move(model_path));
      }

      if constexpr (std::is_same_v<CompressorType,
                                   owl::compression::Compressor>) {
        auto compressor = std::make_unique<owl::compression::Compressor>();
        compressor_ = std::move(compressor);
      }

      this->use_quantization = use_quantization;
      int dimension = embedder_.embedder().value().getDimension();

      if (use_quantization) {
        sq_quantizer = std::make_unique<utils::ScalarQuantizer>();
        pq_quantizer = std::make_unique<utils::ProductQuantizer>(8, 256);
        faiss_index_quantized = std::make_unique<faiss::IndexFlatL2>(dimension);
      } else {
        faiss_index = std::make_unique<faiss::IndexFlatL2>(dimension);
      }

      spdlog::info("VectorFS initialized with {} compressor",
                   std::visit(
                       [](const auto &comp) {
                         return owl::compression::CompressionTraits<
                             std::decay_t<decltype(*comp)>>::Name;
                       },
                       compressor_));

      return true;
    } catch (const std::exception &e) {
      spdlog::error("Failed to initialize VectorFS: {}", e.what());
      return false;
    }
  }

  std::vector<float> get_embedding(const std::string &text);
  std::string get_embedder_info();

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
  void test_markov_chains();
  void test_semantic_search();
};

} // namespace owl::vectorfs

#endif