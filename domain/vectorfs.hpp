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
  static VectorFS *instance_;

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
  std::map<std::string, std::shared_ptr<IKnowledgeContainer>>
      container_adapters_;

  void initialize_container_paths();
  std::shared_ptr<IKnowledgeContainer>
  get_container_for_path(const std::string &path);
  std::string generate_container_listing();
  std::string generate_container_content(const std::string &container_id);
  std::string handle_container_search(const std::string &container_id,
                                      const std::string &query);

  std::string generate_enhanced_search_result(const std::string &query);
  std::string generate_search_result(const std::string &query);
  std::string generate_markov_test_result();

  void initialize_zeromq();
  void process_messages();
  void parse_base_dir();
  bool load_existing_container(const std::string &container_id,
                               const std::string &container_path);
  bool load_container_adapter(const std::string &container_id,
                              const std::string &container_path,
                              const nlohmann::json &config);

  void handle_container_create(const nlohmann::json &message);
  void handle_file_create(const nlohmann::json &message);
  void handle_container_stop(const nlohmann::json &message);
  bool create_container_from_message(const nlohmann::json &message);
  bool create_file_from_message(const nlohmann::json &message);
  bool stop_container_from_message(const nlohmann::json &message);

public:
  VectorFS(State &state) : state_{state}, zmq_context_(1) {
    virtual_dirs.insert("/");
    initialize_container_paths();
    initialize_zeromq();
    instance_ = this;
  }

  ~VectorFS() {
    running_ = false;
    if (message_thread_.joinable()) {
      message_thread_.join();
    }
    instance_ = nullptr;
  }

  void test_semantic_search();
  void test_markov_chains();
  void test_container();

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
    if (!instance_)
      return -ENOENT;
    return instance_->getattr(path, stbuf, fi);
  }

  static inline int readdir_callback(const char *path, void *buf,
                                     fuse_fill_dir_t filler, off_t offset,
                                     struct fuse_file_info *fi,
                                     enum fuse_readdir_flags flags) {
    if (!instance_)
      return -ENOENT;
    return instance_->readdir(path, buf, filler, offset, fi, flags);
  }

  static inline int open_callback(const char *path, struct fuse_file_info *fi) {
    if (!instance_)
      return -ENOENT;
    return instance_->open(path, fi);
  }

  static inline int read_callback(const char *path, char *buf, size_t size,
                                  off_t offset, struct fuse_file_info *fi) {
    if (!instance_)
      return -ENOENT;
    return instance_->read(path, buf, size, offset, fi);
  }

  static inline int write_callback(const char *path, const char *buf,
                                   size_t size, off_t offset,
                                   struct fuse_file_info *fi) {
    if (!instance_)
      return -ENOENT;
    return instance_->write(path, buf, size, offset, fi);
  }

  static inline int mkdir_callback(const char *path, mode_t mode) {
    if (!instance_)
      return -ENOENT;
    return instance_->mkdir(path, mode);
  }

  static inline int create_callback(const char *path, mode_t mode,
                                    struct fuse_file_info *fi) {
    if (!instance_)
      return -ENOENT;
    return instance_->create(path, mode, fi);
  }

  static inline int utimens_callback(const char *path,
                                     const struct timespec tv[2],
                                     struct fuse_file_info *fi) {
    if (!instance_)
      return -ENOENT;
    return instance_->utimens(path, tv, fi);
  }

  static inline int rmdir_callback(const char *path) {
    if (!instance_)
      return -ENOENT;
    return instance_->rmdir(path);
  }

  static inline int unlink_callback(const char *path) {
    if (!instance_)
      return -ENOENT;
    return instance_->unlink(path);
  }

  static inline int getxattr_callback(const char *path, const char *name,
                                      char *value, size_t size) {
    if (!instance_)
      return -ENOENT;
    return instance_->getxattr(path, name, value, size);
  }

  static inline int setxattr_callback(const char *path, const char *name,
                                      const char *value, size_t size,
                                      int flags) {
    if (!instance_)
      return -ENOENT;
    return instance_->setxattr(path, name, value, size, flags);
  }

  static inline int listxattr_callback(const char *path, char *list,
                                       size_t size) {
    if (!instance_)
      return -ENOENT;
    return instance_->listxattr(path, list, size);
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