#ifndef VECTORFS_HPP
#define VECTORFS_HPP

#define FUSE_USE_VERSION 31

#include <atomic>
#include <fuse3/fuse.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>

#include "file/fileinfo.hpp"
#include "knowledge_container.hpp"
#include "state.hpp"
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include <zmq.hpp>

namespace owl::vectorfs {

class VectorFS {
private:
  std::map<std::string, fileinfo::FileInfo> virtual_files;
  std::set<std::string> virtual_dirs;

  State &state_;

  zmq::context_t zmq_context_;
  std::unique_ptr<zmq::socket_t> zmq_subscriber_;
  std::thread message_thread_;
  std::atomic<bool> running_{false};

  struct ContainerInfo {
    std::string container_id;
    std::string user_id;
    std::string status;
    std::string namespace_;
    size_t size;
    bool available;
    std::map<std::string, std::string> labels;
    std::vector<std::string> commands;
  };

  std::map<std::string, ContainerInfo> containers_;
  std::map<std::string,
           std::shared_ptr<KnowledgeContainer<OssecContainerAdapter>>>
      container_adapters_;

  void initialize_container_paths();
  std::shared_ptr<KnowledgeContainer<OssecContainerAdapter>>
  get_container_for_path(const std::string &path);
  std::string generateContainerListing();
  std::string generateContainerContent(const std::string &container_id);
  std::string handle_container_search(const std::string &container_id,
                                      const std::string &query);

  std::string generate_enhanced_search_result(const std::string &query);
  std::string generate_search_result(const std::string &query);
  std::string generate_markov_test_result();

  nlohmann::json handle_get_container_metrics(const nlohmann::json &message);

  void initialize_zeromq();
  void process_messages();
  void parse_base_dir();
  bool load_existing_container(const std::string &container_id,
                               const std::string &container_path);
  bool load_container_adapter(const std::string &container_id,
                              const std::string &container_path,
                              const nlohmann::json &config);

  bool handleContainerCreate(const nlohmann::json &message);
  bool handleFileCreate(const nlohmann::json &message);
  bool handleFileDelete(const nlohmann::json &message);
  bool handleContainerStop(const nlohmann::json &message);
  bool handleContainerDelete(const nlohmann::json &message);

  bool createContainerFromMessage(const nlohmann::json &message);
  bool createFileFromMessage(const nlohmann::json &message);
  bool deleteFileFromMessage(const nlohmann::json &message);
  bool stopContainerFromMessage(const nlohmann::json &message);
  bool deleteContainerFromMessage(const nlohmann::json &message);

public:
  VectorFS(State &state) : state_{state}, zmq_context_(1) {
    virtual_dirs.insert("/");
    initialize_container_paths();
    initialize_zeromq();
  }

  ~VectorFS() {
    running_ = false;
    if (message_thread_.joinable()) {
      message_thread_.join();
    }
  }

  void test_semantic_search();
  void test_markov_chains();

  std::shared_ptr<KnowledgeContainer<OssecContainerAdapter>>
  get_unified_container(const std::string &container_id) {
    auto container = state_.getContainerManager().get_container(container_id);
    if (container) {
      return container;
    }

    auto adapter_it = container_adapters_.find(container_id);
    if (adapter_it != container_adapters_.end()) {
      return adapter_it->second;
    }

    return nullptr;
  }

  std::shared_ptr<KnowledgeContainer<OssecContainerAdapter>>
  get_container_adapter(const std::string &container_id) {
    auto it = container_adapters_.find(container_id);
    return it != container_adapters_.end() ? it->second : nullptr;
  }

  [[nodiscard]] chunkees::Search &getSearch() noexcept {
    return state_.getSearch();
  }

  [[nodiscard]] const chunkees::Search &getSearch() const noexcept {
    return state_.getSearch();
  }

  std::string get_embedder_info() const {
    return state_.getSearch().getEmbedderInfoImpl();
  }

  void rebuild_index() {
    auto result = state_.getSearch().rebuildIndexImpl();
    if (!result.is_ok()) {
      spdlog::warn("Failed to rebuild index: {}", result.error().what());
    }
  }

  size_t get_indexed_files_count() const {
    auto result = state_.getSearch().getIndexedFilesCountImpl();
    return result.is_ok() ? result.value() : 0;
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

  static inline int getattr_callback(const char *path, struct stat *stbuf,
                                     struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->getattr(path, stbuf, fi);
  }

  static inline int readdir_callback(const char *path, void *buf,
                                     fuse_fill_dir_t filler, off_t offset,
                                     struct fuse_file_info *fi,
                                     enum fuse_readdir_flags flags) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->readdir(path, buf, filler, offset, fi, flags);
  }

  static inline int open_callback(const char *path, struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->open(path, fi);
  }

  static inline int read_callback(const char *path, char *buf, size_t size,
                                  off_t offset, struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->read(path, buf, size, offset, fi);
  }

  static inline int write_callback(const char *path, const char *buf,
                                   size_t size, off_t offset,
                                   struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->write(path, buf, size, offset, fi);
  }

  static inline int mkdir_callback(const char *path, mode_t mode) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->mkdir(path, mode);
  }

  static inline int create_callback(const char *path, mode_t mode,
                                    struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->create(path, mode, fi);
  }

  static inline int utimens_callback(const char *path,
                                     const struct timespec tv[2],
                                     struct fuse_file_info *fi) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->utimens(path, tv, fi);
  }

  static inline int rmdir_callback(const char *path) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->rmdir(path);
  }

  static inline int unlink_callback(const char *path) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->unlink(path);
  }

  static inline int getxattr_callback(const char *path, const char *name,
                                      char *value, size_t size) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->getxattr(path, name, value, size);
  }

  static inline int setxattr_callback(const char *path, const char *name,
                                      const char *value, size_t size,
                                      int flags) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->setxattr(path, name, value, size, flags);
  }

  static inline int listxattr_callback(const char *path, char *list,
                                       size_t size) {
    struct fuse_context *context = fuse_get_context();
    if (!context || !context->private_data)
      return -ENOENT;
    
    VectorFS *fs = static_cast<VectorFS*>(context->private_data);
    return fs->listxattr(path, list, size);
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
};

} // namespace owl::vectorfs

#endif