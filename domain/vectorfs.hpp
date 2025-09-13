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

#include "embedded/embedded_fasttext.hpp"
#include "embedded/embedded_base.hpp"
#include "file/fileinfo.hpp"
#include "infrastructure/measure.hpp"

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
  EmbedderVariant embedder_;
  std::string model_type_;
  std::string model_path_;

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
        use_quantization(false) {}

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
    model_path_ = model_path;
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
    if (strncmp(path, "/.search/", 9) == 0) {
      auto start = std::chrono::high_resolution_clock::now();
      std::string query = path + 9;

      query = url_decode(query);
      std::replace(query.begin(), query.end(), '_', ' ');

      std::string content = generate_search_result(query);

      if (offset >= content.size()) {
        return 0;
      }
      size_t len = std::min(content.size() - offset, size);
      memcpy(buf, content.c_str() + offset, len);
      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();

      spdlog::info("Search for '{}' took {} ms", query, duration);
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

  void test_semantic_search() {
    if (!std::holds_alternative<std::unique_ptr<embedded::FastTextEmbedder>>(
            embedder_) ||
        !faiss_index) {
      spdlog::error("Embedder or index not initialized for testing");
      return;
    }
    spdlog::info("=== Тестирование семантического поиска ===");
    virtual_files["/test1.txt"] = fileinfo::FileInfo(
        S_IFREG | 0644, 0, "Программирование на C++ и системное ПО", getuid(),
        getgid(), time(nullptr), time(nullptr), time(nullptr));
    virtual_files["/test2.txt"] = fileinfo::FileInfo(
        S_IFREG | 0644, 0, "Машинное обучение и нейронные сети", getuid(),
        getgid(), time(nullptr), time(nullptr), time(nullptr));
    virtual_files["/test3.txt"] = fileinfo::FileInfo(
        S_IFREG | 0644, 0, "Базы данных и SQL запросы", getuid(), getgid(),
        time(nullptr), time(nullptr), time(nullptr));
    update_embedding("/test1.txt");
    update_embedding("/test2.txt");
    update_embedding("/test3.txt");
    rebuild_index();
    std::vector<std::string> test_queries = {"программирование",
                                             "машинное обучение", "базы данных",
                                             "системное программирование"};
    for (const auto &query : test_queries) {
      spdlog::info("Запрос: '{}'", query);
      auto results = semantic_search(query, 3);
      if (results.empty()) {
        spdlog::warn("  No results found");
      } else {
        for (const auto &[path, score] : results) {
          spdlog::info("  {} (score: {:.3f})", path, score);
          auto it = virtual_files.find(path);
          if (it != virtual_files.end()) {
            spdlog::info("    Content: {}", it->second.content);
          }
        }
      }
      spdlog::info("---");
    }
  }
};

} // namespace vfs::vectorfs

#endif // VECTORFS_HPP