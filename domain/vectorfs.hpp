#ifndef VECTORFS_HPP
#define VECTORFS_HPP

#define FUSE_USE_VERSION 31

#include <chrono>
#include <cstring>
#include <fuse3/fuse.h>
#include <map>
#include <memory>
#include <search.hpp>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "container_manager.hpp"
#include "file/fileinfo.hpp"
#include "shared_memory/shared_memory.hpp"
#include <memory/container_builder.hpp>
#include <spdlog/spdlog.h>

#include "state.hpp"

namespace owl::vectorfs {

class VectorFS {
private:
  std::map<std::string, fileinfo::FileInfo> virtual_files;
  std::set<std::string> virtual_dirs;

  std::unique_ptr<owl::shared::SharedMemoryManager> shm_manager;

  State &state_;

  void updateFromSharedMemory();

  void initialize_container_paths();
  std::shared_ptr<IKnowledgeContainer>
  get_container_for_path(const std::string &path);
  std::string generate_container_listing();
  std::string generate_container_content(const std::string &container_id);
  std::string handle_container_search(const std::string &container_id,
                                      const std::string &query);
  std::string url_decode(const std::string &str);

  std::string generate_enhanced_search_result(const std::string &query);
  std::string generate_search_result(const std::string &query);

  std::string generate_markov_test_result();
  void test_semantic_search();
  void test_markov_chains();
  void test_container();

  static VectorFS *instance_;

public:
  VectorFS(const State &state)
      : state_{state},
        container_manager_(owl::vectorfs::ContainerManager::get_instance()) {
    virtual_dirs.insert("/");
    initialize_container_paths();
    initialize_shared_memory();
    instance_ = this;
  }

  [[nodiscard]] const chunkees::Search &get_search() noexcept {
    return state_.search_;
  }

  void initialize_shared_memory() {
    try {
      shm_manager = std::make_unique<owl::shared::SharedMemoryManager>();
      if (!shm_manager->initialize()) {
        spdlog::warn("Failed to initialize shared memory");
      } else {
        spdlog::info("Shared memory initialized successfully");
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to create shared memory manager: {}", e.what());
    }
  }

  std::string get_embedder_info() const {
    return state_.search_.getEmbedderInfoImpl();
  }

  void rebuild_index() {
    auto result = state_.search_.rebuildIndexImpl();
    if (!result.is_ok()) {
      spdlog::warn("Failed to rebuild index: {}", result.error().what());
    }
  }

  size_t get_indexed_files_count() const {
    auto result = state_.search_.getIndexedFilesCountImpl();
    return result.is_ok() ? result.value() : 0;
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

  ~VectorFS() { instance_ = nullptr; }

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

VectorFS *VectorFS::instance_ = nullptr;

} // namespace owl::vectorfs

#endif