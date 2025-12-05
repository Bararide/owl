#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <search.hpp>
#include <set>
#include <shared_mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include "container_manager.hpp"
#include "env/cppenv.hpp"
#include "file/fileinfo.hpp"
#include <memory/container_builder.hpp>
#include <semantic/semantic_chunker.hpp>
#include <spdlog/spdlog.h>

namespace owl::vectorfs {

class VectorFSState {
public:
  VectorFSState(std::shared_ptr<chunkees::Search> search,
                std::shared_ptr<ContainerManager> container_manager,
                std::shared_ptr<EmbedderManager<>> embedder_manager)
      : search_(std::move(search)),
        container_manager_(std::move(container_manager)),
        embedder_manager_(std::move(embedder_manager)) {

    if (!env_manager_.load_from_file("../.env")) {
      spdlog::error("Error load env file");
    }

    if (embedder_manager_) {
      text_chunker_ =
          std::make_shared<semantic::SemanticChunker<>>(*embedder_manager_);
    } else {
      spdlog::error(
          "EmbedderManager not available for SemanticChunker initialization");
    }

    virtual_dirs_.insert("/");
  }

  std::map<std::string, fileinfo::FileInfo> virtual_files_;
  std::set<std::string> virtual_dirs_;
  mutable std::shared_mutex fs_mutex_;

  cppenv::EnvManager env_manager_;
  std::shared_ptr<chunkees::Search> search_;
  std::shared_ptr<ContainerManager> container_manager_;
  std::shared_ptr<EmbedderManager<>> embedder_manager_;
  std::shared_ptr<semantic::SemanticChunker<>> text_chunker_;

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
  mutable std::shared_mutex containers_mutex_;

  bool file_exists(const std::string &path) const {
    std::shared_lock lock(fs_mutex_);
    return virtual_files_.find(path) != virtual_files_.end();
  }

  bool dir_exists(const std::string &path) const {
    std::shared_lock lock(fs_mutex_);
    return virtual_dirs_.find(path) != virtual_dirs_.end();
  }

  const fileinfo::FileInfo *get_file(const std::string &path) const {
    std::shared_lock lock(fs_mutex_);
    auto it = virtual_files_.find(path);
    return it != virtual_files_.end() ? &it->second : nullptr;
  }

  void add_file(const std::string &path, const fileinfo::FileInfo &file) {
    std::unique_lock lock(fs_mutex_);
    virtual_files_[path] = file;
  }

  void remove_file(const std::string &path) {
    std::unique_lock lock(fs_mutex_);
    virtual_files_.erase(path);
  }

  void add_dir(const std::string &path) {
    std::unique_lock lock(fs_mutex_);
    virtual_dirs_.insert(path);
  }

  void remove_dir(const std::string &path) {
    std::unique_lock lock(fs_mutex_);
    virtual_dirs_.erase(path);
  }

  void list_directory(const std::string &path, std::vector<std::string> &files,
                      std::vector<std::string> &dirs) const {
    std::shared_lock lock(fs_mutex_);
    for (const auto &dir : virtual_dirs_) {
      if (dir.rfind(path, 0) == 0 && dir != path) {
        std::string rel_path = dir.substr(path == "/" ? 1 : path.length() + 1);
        if (rel_path.find('/') == std::string::npos) {
          dirs.push_back(rel_path);
        }
      }
    }

    for (const auto &file : virtual_files_) {
      if (file.first.rfind(path, 0) == 0) {
        std::string rel_path =
            file.first.substr(path == "/" ? 1 : path.length() + 1);
        if (rel_path.find('/') == std::string::npos) {
          files.push_back(rel_path);
        }
      }
    }
  }

  chunkees::Search &getSearch() {
    if (!search_)
      throw std::runtime_error("Search not initialized");
    return *search_;
  }

  ContainerManager &getContainerManager() {
    if (!container_manager_) {
      throw std::runtime_error("ContainerManager not initialized");
    }
    return *container_manager_;
  }

  EmbedderManager<> &getEmbedderManager() {
    if (!embedder_manager_) {
      throw std::runtime_error("EmbedderManager<> not initialized");
    }
    return *embedder_manager_;
  }

  semantic::SemanticChunker<> &getSemanticChunker() {
    if (!text_chunker_) {
      throw std::runtime_error("SemanticChunker<> not initialized");
    }
    return *text_chunker_;
  }
};

} // namespace owl::vectorfs

#endif // VECTORFS_STATE_HPP