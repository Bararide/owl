
#include "vectorfs.hpp"

namespace owl::vectorfs {

VectorFS *VectorFS::instance_ = nullptr;

void VectorFS::initialize_container_paths() {
  virtual_dirs.insert("/.containers");
  virtual_dirs.insert("/.containers/.all");
  virtual_dirs.insert("/.containers/.search");
}

std::shared_ptr<IKnowledgeContainer>
VectorFS::get_container_for_path(const std::string &path) {
  if (path.find("/.containers/") == 0) {
    size_t start = strlen("/.containers/");
    size_t end = path.find('/', start);
    std::string container_id = path.substr(start, end - start);
    return state_.get_container_manager().get_container(container_id);
  }
  return nullptr;
}

std::string VectorFS::generate_container_listing() {
  std::stringstream ss;
  ss << "=== Knowledge Containers ===\n\n";
  auto containers = state_.get_container_manager().get_all_containers();
  for (const auto &container : containers) {
    ss << "Container: " << container->get_id() << "\n";
    ss << "  Owner: " << container->get_owner() << "\n";
    ss << "  Namespace: " << container->get_namespace() << "\n";
    ss << "  Status: " << container->get_status() << "\n";
    ss << "  Size: " << container->get_size() << " bytes\n";
    ss << "  Available: " << (container->is_available() ? "yes" : "no") << "\n";
    auto labels = container->get_labels();
    if (!labels.empty()) {
      ss << "  Labels:\n";
      for (const auto &[key, value] : labels)
        ss << "    " << key << ": " << value << "\n";
    }
    ss << "\n";
  }
  ss << "Total: " << containers.size() << " containers\n";
  ss << "Available: "
     << state_.get_container_manager().get_available_container_count()
     << " containers\n";
  return ss.str();
}

std::string
VectorFS::generate_container_content(const std::string &container_id) {
  auto container = state_.get_container_manager().get_container(container_id);
  if (!container) {
    return "Container not found: " + container_id;
  }

  std::stringstream ss;
  ss << "=== Container: " << container_id << " ===\n\n";
  ss << "Owner: " << container->get_owner() << "\n";
  ss << "Namespace: " << container->get_namespace() << "\n";
  ss << "Status: " << container->get_status() << "\n";
  ss << "Size: " << container->get_size() << " bytes\n";
  ss << "Available: " << (container->is_available() ? "yes" : "no") << "\n\n";

  auto files = container->list_files(container->get_data_path());
  if (files.empty()) {
    ss << "No files in container\n";
  } else {
    ss << "Files:\n";
    for (const auto &file : files) {
      ss << "  - " << file << "\n";
    }
  }

  return ss.str();
}

std::string VectorFS::handle_container_search(const std::string &container_id,
                                              const std::string &query) {
  auto container = state_.get_container_manager().get_container(container_id);
  if (!container) {
    return "Container not found: " + container_id;
  }

  std::stringstream ss;
  ss << "=== Search in Container: " << container_id << " ===\n\n";
  ss << "Query: " << query << "\n\n";

  ss << "Search functionality for containers is under development.\n";
  ss << "Available files:\n";

  auto files = container->list_files(container->get_data_path());
  for (const auto &file : files) {
    ss << "  - " << file << "\n";
  }

  return ss.str();
}

int VectorFS::getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/.search") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    return 0;
  }

  if (strcmp(path, "/.debug") == 0 || strcmp(path, "/.all") == 0 ||
      strcmp(path, "/.markov") == 0 || strcmp(path, "/.reindex") == 0 ||
      strcmp(path, "/.embeddings") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1024;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
    return 0;
  }

  if (strncmp(path, "/.search/", 9) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1024;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
    return 0;
  }

  if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end == std::string::npos) {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
      return 0;
    } else {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string container_path = path_str.substr(container_end);

      auto container =
          state_.get_container_manager().get_container(container_id);
      if (container) {
        if (container_path == "/.search" || container_path == "/.debug" ||
            container_path == "/.all") {

          if (container_path == "/.search") {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
          } else {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = 4096;
          }

          stbuf->st_uid = getuid();
          stbuf->st_gid = getgid();
          stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
          return 0;
        }

        if (strncmp(container_path.c_str(), "/.search/", 9) == 0) {
          stbuf->st_mode = S_IFREG | 0444;
          stbuf->st_nlink = 1;
          stbuf->st_size = 4096;
          stbuf->st_uid = getuid();
          stbuf->st_gid = getgid();
          stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
          return 0;
        }

        if (container_path == container->get_data_path() || container_path.empty()) {
          stbuf->st_mode = S_IFDIR | 0555;
          stbuf->st_nlink = 2;
        } else if (container->file_exists(container_path)) {
          stbuf->st_mode = S_IFREG | 0444;
          stbuf->st_nlink = 1;
          std::string content = container->get_file_content(container_path);
          stbuf->st_size = content.size();
        } else {
          auto files = container->list_files(container_path);
          if (!files.empty()) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
          } else {
            return -ENOENT;
          }
        }
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
        return 0;
      }
    }
    return -ENOENT;
  }

  if (strcmp(path, "/.containers/.all") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 4096;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
    return 0;
  }

  if (strcmp(path, "/.containers/.search") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    return 0;
  }

  if (virtual_dirs.count(path)) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
    return 0;
  }

  auto it = virtual_files.find(path);
  if (it != virtual_files.end()) {
    const auto &fi = it->second;
    stbuf->st_mode = fi.mode;
    stbuf->st_nlink = 1;
    stbuf->st_size = fi.content.size();
    stbuf->st_uid = fi.uid;
    stbuf->st_gid = fi.gid;
    stbuf->st_atime = fi.access_time;
    stbuf->st_mtime = fi.modification_time;
    stbuf->st_ctime = fi.modification_time;
    return 0;
  }

  std::string requested_file = std::filesystem::path(path).filename().string();
  for (const auto &[file_path, file_info] : virtual_files) {
    std::string current_filename =
        std::filesystem::path(file_path).filename().string();
    if (current_filename == requested_file) {
      const auto &fi = file_info;
      stbuf->st_mode = fi.mode;
      stbuf->st_nlink = 1;
      stbuf->st_size = fi.content.size();
      stbuf->st_uid = fi.uid;
      stbuf->st_gid = fi.gid;
      stbuf->st_atime = fi.access_time;
      stbuf->st_mtime = fi.modification_time;
      stbuf->st_ctime = fi.modification_time;
      return 0;
    }
  }

  return -ENOENT;
}

int VectorFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {

  filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
  filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

  if (strcmp(path, "/") == 0) {
    filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".reindex", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".embeddings", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".markov", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".containers", nullptr, 0, FUSE_FILL_DIR_PLUS);

    for (const auto &file : virtual_files) {
      std::string file_name =
          std::filesystem::path(file.first).filename().string();
      if (!file_name.empty()) {
        filler(buf, file_name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    }
  } else if (strcmp(path, "/.containers") == 0) {

    filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);

    spdlog::info("Reading /containers directory, found containers:");

    for (const auto &[container_id, container_info] : containers_) {
      spdlog::info("  - Container from ZeroMQ: {} (status: {})", container_id,
                   container_info.status);
      filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    auto containers = state_.get_container_manager().get_all_containers();
    for (const auto &container : containers) {
      std::string container_id = container->get_id();
      if (containers_.find(container_id) == containers_.end()) {
        spdlog::info("  - Container from manager: {}", container_id);
        filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    }
  } else if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end == std::string::npos) {
      std::string container_id = path_str.substr(container_start);

      auto container_it = container_adapters_.find(container_id);
      if (container_it != container_adapters_.end()) {
        auto container = container_it->second;

        filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);

        auto files = container->list_files(container->get_data_path());
        for (const auto &file : files) {
          filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        }

        spdlog::info(
            "Container {} directory listing: special files + {} real files",
            container_id, files.size());
      } else {
        auto container =
            state_.get_container_manager().get_container(container_id);
        if (container) {
          filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);

          auto files = container->list_files(container->get_data_path());
          for (const auto &file : files) {
            filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
          }
        }
      }
    } else {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string container_path = path_str.substr(container_end);

      auto container_it = container_adapters_.find(container_id);
      if (container_it != container_adapters_.end()) {
        auto container = container_it->second;

        if (container_path == "/.search") {
          filler(buf, "test", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "sql", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "neural", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "python", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "cpp", nullptr, 0, FUSE_FILL_DIR_PLUS);
        } else {
          auto files = container->list_files(container_path);
          for (const auto &file : files) {
            filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
          }
        }
      } else {
        auto container =
            state_.get_container_manager().get_container(container_id);
        if (container) {
          if (container_path == "/.search") {
            filler(buf, "test", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "sql", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "neural", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "python", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "cpp", nullptr, 0, FUSE_FILL_DIR_PLUS);
          } else {
            auto files = container->list_files(container_path);
            for (const auto &file : files) {
              filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
            }
          }
        }
      }
    }
  } else if (strcmp(path, "/.containers/.search") == 0) {
    for (const auto &[container_id, container_info] : containers_) {
      filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }
    auto containers = state_.get_container_manager().get_all_containers();
    for (const auto &container : containers) {
      std::string container_id = container->get_id();
      if (containers_.find(container_id) == containers_.end()) {
        filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    }
  } else if (strcmp(path, "/.search") == 0) {
    filler(buf, "test", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "sql", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "neural", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "python", nullptr, 0, FUSE_FILL_DIR_PLUS);
  }

  return 0;
}

int VectorFS::read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {

  if (strcmp(path, "/.containers/.all") == 0) {
    std::string content = generate_container_listing();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.debug") == 0) {
    std::stringstream ss;
    ss << "=== DEBUG INFO ===\n";
    ss << "Total virtual_files: " << virtual_files.size() << "\n";
    ss << "Total containers: "
       << state_.get_container_manager().get_container_count() << "\n";
    ss << "Available containers: "
       << state_.get_container_manager().get_available_container_count()
       << "\n";

    std::string content = ss.str();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.all") == 0) {
    std::stringstream ss;
    ss << "=== All Files ===\n\n";

    for (const auto &[file_path, file_info] : virtual_files) {
      ss << "VIRT: " << file_path << " (" << file_info.size << " bytes)\n";
    }

    std::string content = ss.str();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.reindex") == 0) {
    rebuild_index();

    std::stringstream ss;
    ss << "Forcing reindex...\n";
    ss << "Reindex completed!\n";
    ss << "Indexed files: " << get_indexed_files_count() << "\n";

    std::string content = ss.str();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end != std::string::npos) {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string container_path = path_str.substr(container_end);

      auto container =
          state_.get_container_manager().get_container(container_id);
      if (!container) {
        return -ENOENT;
      }

      if (strncmp(container_path.c_str(), "/.search/", 9) == 0) {
        std::string query = container_path.substr(9);
        query = url_decode(query);
        std::replace(query.begin(), query.end(), '_', ' ');

        std::stringstream ss;
        ss << "=== Semantic Search in Container: " << container_id
           << " ===\n\n";
        ss << "Query: " << query << "\n\n";

        auto search_results = container->semantic_search(query, 10);

        if (search_results.empty()) {
          auto pattern_results = container->search_files(query);
          if (pattern_results.empty()) {
            ss << "No results found in container.\n";
          } else {
            ss << "📊 Pattern Search Results:\n";
            for (const auto &result : pattern_results) {
              ss << "📄 " << result << "\n";

              std::string file_content = container->get_file_content(result);
              if (!file_content.empty()) {
                std::string preview = file_content.substr(0, 100);
                if (file_content.size() > 100)
                  preview += "...";
                ss << "   Content: " << preview << "\n";
              }
              ss << "\n";
            }
          }
        } else {
          ss << "🎯 Semantic Search Results:\n";
          for (const auto &result : search_results) {
            ss << "📄 " << result << "\n";

            std::string file_content = container->get_file_content(result);
            if (!file_content.empty()) {
              std::string preview = file_content.substr(0, 100);
              if (file_content.size() > 100)
                preview += "...";
              ss << "   Content: " << preview << "\n";
            }

            std::string category = container->classify_file(result);
            ss << "   Category: " << category << "\n\n";
          }

          if (!search_results.empty()) {
            auto recommendations =
                container->get_recommendations(search_results[0], 3);
            if (!recommendations.empty()) {
              ss << "💡 Recommended Files:\n";
              for (const auto &rec : recommendations) {
                ss << "   → " << rec << "\n";
              }
              ss << "\n";
            }
          }
        }

        std::string content = ss.str();
        if (offset >= static_cast<off_t>(content.size())) {
          return 0;
        }
        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      }

      if (container_path == "/.all") {
        std::stringstream ss;
        ss << "=== All Files in Container: " << container_id << " ===\n\n";

        auto files = container->list_files(container->get_data_path());
        for (const auto &file : files) {
          ss << file << "\n";
        }

        std::string content = ss.str();
        if (offset >= static_cast<off_t>(content.size())) {
          return 0;
        }
        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      }

      if (container_path == "/.debug") {
        std::stringstream ss;
        ss << "=== Debug Info for Container: " << container_id << " ===\n\n";
        ss << "Owner: " << container->get_owner() << "\n";
        ss << "Namespace: " << container->get_namespace() << "\n";
        ss << "Status: " << container->get_status() << "\n";
        ss << "Size: " << container->get_size() << " bytes\n";
        ss << "Available: " << (container->is_available() ? "yes" : "no")
           << "\n";
        ss << "\nCommands:\n";
        for (const auto &cmd : container->get_commands()) {
          ss << "  - " << cmd << "\n";
        }
        ss << "\nLabels:\n";
        for (const auto &[key, value] : container->get_labels()) {
          ss << "  " << key << ": " << value << "\n";
        }

        std::string content = ss.str();
        if (offset >= static_cast<off_t>(content.size())) {
          return 0;
        }
        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      }

      if (container->file_exists(container_path)) {
        std::string content = container->get_file_content(container_path);
        if (offset >= static_cast<off_t>(content.size())) {
          return 0;
        }
        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      }
    }
    return -ENOENT;
  }

  if (strcmp(path, "/.markov") == 0) {
    // test_markov_chains();
    std::string content = generate_markov_test_result();

    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.embeddings") == 0) {
    std::stringstream ss;
    ss << "=== Embeddings Report ===\n\n";
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
        for (int i = 0; i < std::min(5, (int)file_info.embedding.size()); ++i) {
          ss << file_info.embedding[i] << " ";
        }
        ss << "\n";
      }
    }
    ss << "Total with embeddings: " << count << "\n";
    ss << "Embedder: " << get_embedder_info() << "\n";

    std::string content = ss.str();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strncmp(path, "/.search/", 9) == 0) {
    std::string query = path + 9;
    query = url_decode(query);
    std::replace(query.begin(), query.end(), '_', ' ');

    std::string content;

    try {
      content = generate_enhanced_search_result(query);
    } catch (const std::exception &e) {
      spdlog::error("Enhanced search failed: {}, falling back to basic search",
                    e.what());
      content = generate_search_result(query);
    }

    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  auto it = virtual_files.find(path);
  if (it != virtual_files.end()) {
    const std::string &content = it->second.content;
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  return -ENOENT;
}

int VectorFS::rmdir(const char *path) {
  if (virtual_dirs.count(path) == 0) {
    return -ENOENT;
  }

  std::string dir = path;
  if (dir == "/")
    dir = "";

  for (auto it = virtual_files.begin(); it != virtual_files.end();) {
    if (it->first.find(dir) == 0) {
      it = virtual_files.erase(it);
      rebuild_index();
    } else {
      ++it;
    }
  }

  virtual_dirs.erase(path);
  return 0;
}

int VectorFS::mkdir(const char *path, mode_t mode) {
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

int VectorFS::open(const char *path, struct fuse_file_info *fi) {
  if (strncmp(path, "/.containers", 12) == 0) {
    fi->fh = 0;
    return 0;
  }

  if (strncmp(path, "/.search/", 9) == 0) {
    fi->fh = 0;
    return 0;
  }

  if (strncmp(path, "/.markov", 8) == 0) {
    fi->fh = 0;
    return 0;
  }

  if (strcmp(path, "/.reindex") == 0 || strcmp(path, "/.embeddings") == 0 ||
      strcmp(path, "/.all") == 0 || strcmp(path, "/.debug") == 0) {
    fi->fh = 0;
    return 0;
  }

  auto it = virtual_files.find(path);
  if (it == virtual_files.end()) {
    return -ENOENT;
  }

  fi->fh = reinterpret_cast<uint64_t>(&it->second);
  return 0;
}

int VectorFS::create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  if (virtual_files.count(path) > 0 || virtual_dirs.count(path) > 0) {
    spdlog::error("File already exists: {}", path);
    return -EEXIST;
  }

  time_t now = time(nullptr);
  auto &file_info = virtual_files[path];
  file_info = fileinfo::FileInfo(S_IFREG | 0644, 0, "", getuid(), getgid(), now,
                                 now, now);

  fi->fh = reinterpret_cast<uint64_t>(&file_info);

  return 0;
}

int VectorFS::utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi) {
  auto it = virtual_files.find(path);
  if (it == virtual_files.end()) {
    return -ENOENT;
  }

  it->second.access_time = tv[0].tv_sec;
  it->second.modification_time = tv[1].tv_sec;

  return 0;
}

int VectorFS::write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  if (fi->fh == 0) {
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

    (void)state_.get_search().updateEmbedding(path);
    rebuild_index();

    return size;
  }

  auto *file_info = reinterpret_cast<fileinfo::FileInfo *>(fi->fh);

  if ((file_info->mode & S_IWUSR) == 0) {
    return -EACCES;
  }

  if (offset + size > file_info->content.size()) {
    file_info->content.resize(offset + size);
  }

  memcpy(&file_info->content[offset], buf, size);
  file_info->size = file_info->content.size();
  file_info->modification_time = time(nullptr);
  file_info->access_time = file_info->modification_time;

  (void)state_.get_search().updateEmbedding(path);
  rebuild_index();

  return size;
}

int VectorFS::unlink(const char *path) {
  if (virtual_files.erase(path) == 0) {
    return -ENOENT;
  }
  rebuild_index();
  return 0;
}

int VectorFS::setxattr(const char *path, const char *name, const char *value,
                       size_t size, int flags) {
  return -ENOTSUP;
}

int VectorFS::getxattr(const char *path, const char *name, char *value,
                       size_t size) {
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

int VectorFS::listxattr(const char *path, char *list, size_t size) {
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
} // namespace owl::vectorfs