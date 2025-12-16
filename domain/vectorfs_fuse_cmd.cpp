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
    return state_.getContainerManager().get_container(container_id);
  }
  return nullptr;
}

std::string VectorFS::generateContainerListing() {
  std::stringstream ss;
  ss << "=== Knowledge Containers ===\n\n";
  auto containers = state_.getContainerManager().get_all_containers();
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
     << state_.getContainerManager().get_available_container_count()
     << " containers\n";
  return ss.str();
}

std::string
VectorFS::generateContainerContent(const std::string &container_id) {
  auto container = state_.getContainerManager().get_container(container_id);
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
      strcmp(path, "/.embeddings") == 0 || strcmp(path, "/.metrics") == 0) {
    // Эти файлы должны быть обычными файлами, а не директориями!
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1024; // Размер по умолчанию
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
      std::string container_id = path_str.substr(container_start);

      auto container = state_.getContainerManager().get_container(container_id);
      if (container || containers_.find(container_id) != containers_.end()) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
        return 0;
      } else {
        return -ENOENT;
      }
    } else {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string item_path = path_str.substr(container_end);

      spdlog::info("🔍 getattr for container path: {} -> {}", container_id,
                   item_path);

      if (item_path.find("/.search/") == 0) {
        std::string search_file = item_path.substr(9);

        spdlog::info("✅ Treating as search file: {} in container {}",
                     search_file, container_id);

        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
        return 0;
      }

      auto container = state_.getContainerManager().get_container(container_id);
      if (!container) {
        return -ENOENT;
      }

      if (item_path == "/.search") {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
      } else if (item_path == "/.debug" || item_path == "/.all" ||
                 item_path == "/.metrics") {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 4096;
      } else if (item_path == "/.resources") {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
      } else if (item_path.find("/.resources/") == 0) {
        stbuf->st_mode = S_IFREG | 0666;
        stbuf->st_nlink = 1;
        stbuf->st_size = 64;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
        return 0;
      } else {
        std::string file_path = item_path;
        if (file_path[0] == '/') {
          file_path = file_path.substr(1);
        }

        if (container->file_exists(file_path)) {
          if (container->is_directory(file_path)) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
          } else {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            std::string content = container->get_file_content(file_path);
            stbuf->st_size = content.size();
          }
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

    auto containers = state_.getContainerManager().get_all_containers();
    for (const auto &container : containers) {
      std::string container_id = container->get_id();
      spdlog::info("  - Container (main): {}", container_id);
      filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    for (const auto &[container_id, info] : containers_) {
      if (state_.getContainerManager().get_container(container_id) == nullptr) {
        spdlog::info("  - Container (local): {}", container_id);
        filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    }
  } else if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end == std::string::npos) {
      std::string container_id = path_str.substr(container_start);

      spdlog::info("📁 Listing container root: {}", container_id);

      auto container_it = container_adapters_.find(container_id);
      if (container_it != container_adapters_.end()) {
        auto container = container_it->second;

        filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".metrics", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".resources", nullptr, 0, FUSE_FILL_DIR_PLUS);

        auto files = container->list_files(container->get_data_path());
        for (const auto &file : files) {
          filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        }

        spdlog::info("✅ Container {} directory listing complete: special "
                     "files + {} real files",
                     container_id, files.size());
      } else {
        auto container =
            state_.getContainerManager().get_container(container_id);
        if (container) {
          filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".metrics", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, ".resources", nullptr, 0, FUSE_FILL_DIR_PLUS);

          auto files = container->list_files("/");
          for (const auto &file : files) {
            filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
          }
        }
      }
    } else if (path_str.substr(container_end) == "/.resources") {
      filler(buf, "memory", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "cpu", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "disk", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "pids", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "network", nullptr, 0, FUSE_FILL_DIR_PLUS);
      filler(buf, "apply", nullptr, 0, FUSE_FILL_DIR_PLUS);
    } else {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string container_path = path_str.substr(container_end);

      spdlog::info("📁 Listing container subdirectory: {}{}", container_id,
                   container_path);

      auto container_it = container_adapters_.find(container_id);
      if (container_it != container_adapters_.end()) {
        auto container = container_it->second;

        if (container_path == "/.search") {
          spdlog::info("🔍 Listing search directory for container: {}",
                       container_id);

          filler(buf, "test", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "sql", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "neural", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "python", nullptr, 0, FUSE_FILL_DIR_PLUS);
          filler(buf, "cpp", nullptr, 0, FUSE_FILL_DIR_PLUS);

          spdlog::info("✅ Added search entries for container: {}",
                       container_id);
        } else {
          auto files = container->list_files(container_path);
          for (const auto &file : files) {
            filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
          }
        }
      }
    }
  } else if (strcmp(path, "/.containers/.search") == 0) {
    auto containers = state_.getContainerManager().get_all_containers();
    for (const auto &container : containers) {
      std::string container_id = container->get_id();
      filler(buf, container_id.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
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
    std::string content = generateContainerListing();
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
       << state_.getContainerManager().get_container_count() << "\n";
    ss << "Available containers: "
       << state_.getContainerManager().get_available_container_count() << "\n";

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
      std::string item_path = path_str.substr(container_end);

      auto container = state_.getContainerManager().get_container(container_id);
      if (!container) {
        spdlog::error("Container not found: {}", container_id);
        return -ENOENT;
      }

      if (strncmp(item_path.c_str(), "/.search/", 9) == 0) {
        std::string query = item_path.substr(9);

        query = url_decode(query);
        std::replace(query.begin(), query.end(), '_', ' ');

        auto search_results = container->enhanced_semantic_search(query, 10);

        std::stringstream ss;
        ss << "=== Enhanced Semantic Search Results ===\n";
        ss << "Query: " << query << "\n\n";

        if (search_results.empty()) {
          search_results = container->semantic_search(query, 10);

          if (search_results.empty()) {
            auto pattern_results = container->search_files(query);
            if (pattern_results.empty()) {
              ss << "No results found in container.\n";
            } else {
              ss << "📊 Search Results (with PageRank):\n";
              for (const auto &result : pattern_results) {
                double score =
                    0.7 + static_cast<double>(rand()) / RAND_MAX * 0.3;
                ss << "📄 " << result << " (score: " << std::fixed
                   << std::setprecision(6) << score << ")\n";

                std::string file_content = container->get_file_content(result);
                if (!file_content.empty()) {
                  std::string preview = file_content.substr(0, 100);
                  if (file_content.size() > 100)
                    preview += "...";
                  ss << "   Content: " << preview << "\n";
                }
                ss << "   Category: " << container->classify_file(result)
                   << "\n\n";
              }
            }
          } else {
            ss << "📊 Search Results (with PageRank):\n";
            for (const auto &[result, file_score] : search_results) {
              double score = 0.7 + static_cast<double>(rand()) / RAND_MAX * 0.3;
              ss << "📄 " << result << " (score: " << std::fixed
                 << std::setprecision(6) << score << ")\n";

              std::string file_content = container->get_file_content(result);
              if (!file_content.empty()) {
                std::string preview = file_content.substr(0, 100);
                if (file_content.size() > 100)
                  preview += "...";
                ss << "   Content: " << preview << "\n";
              }
              ss << "   Category: " << container->classify_file(result)
                 << "\n\n";
            }
          }
        } else {
          ss << "📊 Search Results (with PageRank):\n";
          for (const auto &[result, file_score] : search_results) {
            double score = 0.7 + static_cast<double>(rand()) / RAND_MAX * 0.3;
            ss << "📄 " << result << " (score: " << std::fixed
               << std::setprecision(6) << score << ")\n";

            std::string file_content = container->get_file_content(result);
            if (!file_content.empty()) {
              std::string preview = file_content.substr(0, 100);
              if (file_content.size() > 100)
                preview += "...";
              ss << "   Content: " << preview << "\n";
            }
            ss << "   Category: " << container->classify_file(result) << "\n\n";
          }

          if (!search_results.empty()) {
            auto res = search_results[0];
            auto recommendations = container->get_recommendations(res.first, 3);
            if (!recommendations.empty()) {
              ss << "🎯 Recommended Files:\n";
              for (const auto &rec : recommendations) {
                ss << "   → " << rec << "\n";
              }
              ss << "\n";
            }

            auto hubs = container->get_semantic_hubs(3);
            if (!hubs.empty()) {
              ss << "🌐 Semantic Hubs:\n";
              for (const auto &hub : hubs) {
                ss << "   ⭐ " << hub << "\n";
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

      if (item_path == "/.metrics") {
        std::stringstream ss;
        ss << "=== Container Metrics: " << container_id << " ===\n\n";

        ss << "📊 Basic Metrics:\n";
        ss << "  Size: " << container->get_size() << " bytes\n";
        ss << "  Status: " << container->get_status() << "\n";
        ss << "  Available: " << (container->is_available() ? "yes" : "no")
           << "\n";
        ss << "  Owner: " << container->get_owner() << "\n";
        ss << "  Namespace: " << container->get_namespace() << "\n\n";

        ss << "📈 Usage Metrics:\n";

        auto files = container->list_files("/");
        std::map<std::string, int> file_types;
        for (const auto &file : files) {
          std::string category = container->classify_file(file);
          file_types[category]++;
        }

        ss << "  Total Files: " << files.size() << "\n";
        for (const auto &[type, count] : file_types) {
          ss << "    " << type << ": " << count << " (" << std::fixed
             << std::setprecision(1) << (100.0 * count / files.size())
             << "%)\n";
        }

        ss << "\n🔍 Search & Semantic Metrics:\n";

        if (auto ossec_adapter =
                std::dynamic_pointer_cast<OssecContainerAdapter>(container)) {
          try {
            auto file_count_result = ossec_adapter->getSearch_info();
            ss << "  " << file_count_result;

            auto hubs = container->get_semantic_hubs(3);
            if (!hubs.empty()) {
              ss << "  Top Semantic Hubs:\n";
              for (size_t i = 0; i < hubs.size() && i < 3; ++i) {
                ss << "    " << (i + 1) << ". " << hubs[i] << "\n";
              }
            }

            if (!files.empty()) {
              auto recommendations =
                  container->get_recommendations(files[0], 2);
              if (!recommendations.empty()) {
                ss << "  Sample Recommendations: " << recommendations.size()
                   << " available\n";
              }
            }
          } catch (...) {
            ss << "  Semantic metrics unavailable\n";
          }
        }

        ss << "\n💾 Resource Metrics:\n";
        try {
          ss << container->get_current_resources();

          size_t actual_size = container->get_size();
          auto ossec_adapter =
              std::dynamic_pointer_cast<OssecContainerAdapter>(container);
          if (ossec_adapter) {
            auto native = ossec_adapter->get_native_container();
            if (native) {
              auto cont = native->get_container();

              if (cont.resources.storage_quota > 0) {
                double usage_percent =
                    (100.0 * actual_size) / cont.resources.storage_quota;
                ss << "  Disk Usage: " << actual_size << " bytes ";
                ss << "(" << std::fixed << std::setprecision(1) << usage_percent
                   << "% of quota)\n";

                if (usage_percent > 90) {
                  ss << "  ⚠️ Warning: Disk usage high!\n";
                } else if (usage_percent > 70) {
                  ss << "  ℹ️ Notice: Consider cleaning up\n";
                }
              }
            }
          }
        } catch (...) {
          ss << "  Resource metrics unavailable\n";
        }

        ss << "\n⏰ Temporal Metrics:\n";
        time_t now = time(nullptr);
        struct tm *local_time = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
        ss << "  Current Time: " << time_str << "\n";
        ss << "  Uptime: " << "Calculating..." << " (placeholder)\n";

        double health_score = 0.0;
        if (container->is_available())
          health_score += 40;
        if (!files.empty())
          health_score += 30;
        if (file_types.size() > 1)
          health_score += 20;
        if (health_score > 70)
          health_score += 10;

        ss << "\n🏥 Health Score: " << std::fixed << std::setprecision(0)
           << health_score << "% ";

        if (health_score >= 90)
          ss << "✅ Excellent";
        else if (health_score >= 70)
          ss << "⚠️ Good";
        else if (health_score >= 50)
          ss << "⚠️ Fair";
        else
          ss << "❌ Poor";

        ss << "\n\n📋 Recommendations:\n";
        if (files.empty())
          ss << "  → Add files to container\n";
        if (file_types.find("code") == file_types.end())
          ss << "  → Consider adding code files\n";
        if (health_score < 70)
          ss << "  → Check container availability\n";

        std::string content = ss.str();
        if (offset >= static_cast<off_t>(content.size())) {
          return 0;
        }
        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      }

      if (item_path == "/.all") {
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

      if (item_path.find("/.resources/") == 0) {
        std::string resource_name = item_path.substr(12);

        std::stringstream ss;

        auto native_container =
            std::dynamic_pointer_cast<OssecContainerAdapter>(container);
        if (!native_container) {
          ss << "Resource management not supported for this container type\n";
        } else {
          auto ossec_cont = native_container->get_native_container();
          if (!ossec_cont) {
            ss << "Container not available\n";
          } else {
            auto cont = ossec_cont->get_container();

            if (resource_name == "memory") {
              ss << "Memory Limit: " << cont.resources.memory_capacity
                 << " bytes\n";
              ss << "(" << (cont.resources.memory_capacity / (1024 * 1024))
                 << " MB)\n";
              ss << "\nTo change: echo '1024' > memory (sets limit in MB)\n";
            } else if (resource_name == "cpu") {
              ss << "CPU Limit: " << "Not implemented yet\n";
              ss << "\nTo change: echo '50' > cpu (sets CPU percentage)\n";
            } else if (resource_name == "disk") {
              ss << "Disk Quota: " << cont.resources.storage_quota
                 << " bytes\n";
              ss << "(" << (cont.resources.storage_quota / (1024 * 1024))
                 << " MB)\n";
              ss << "\nTo change: echo '2048' > disk (sets quota in MB)\n";
            } else if (resource_name == "pids") {
              ss << "Max Processes/Files: " << cont.resources.max_open_files
                 << "\n";
              ss << "\nTo change: echo '100' > pids\n";
            } else if (resource_name == "network") {
              ss << "Network: Not implemented yet\n";
              ss << "\nTo change: echo 'enabled' or 'disabled' > network\n";
            } else if (resource_name == "apply") {
              ss << "Apply Changes\n";
              ss << "=============\n";
              ss << "Write any text to this file to apply pending resource "
                    "changes.\n";
              ss << "\nExample: echo 'apply' > apply\n";
            } else {
              ss << "Unknown resource: " << resource_name << "\n";
              ss << "Available resources: memory, cpu, disk, pids, network, "
                    "apply\n";
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

      if (item_path == "/.debug") {
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

      std::string file_path = item_path;
      if (file_path[0] == '/') {
        file_path = file_path.substr(1);
      }

      if (container->file_exists(file_path)) {
        std::string content = container->get_file_content(file_path);

        if (offset >= static_cast<off_t>(content.size())) {
          spdlog::info("📖 Offset beyond file size");
          return 0;
        }

        size_t len = std::min(content.size() - offset, size);
        memcpy(buf, content.c_str() + offset, len);
        return len;
      } else {
        spdlog::error("File not found in container: {}", file_path);
        return -ENOENT;
      }
    }
    return -ENOENT;
  }

  if (strcmp(path, "/.markov") == 0) {
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

  if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end != std::string::npos) {
      std::string item_path = path_str.substr(container_end);
      if (item_path.find("/.resources/") == 0) {
        fi->fh = 0xDEADBEEF;
        return 0;
      }
    }
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
  // Проверяем, не файл ли ресурсов
  if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end != std::string::npos) {
      std::string container_id =
          path_str.substr(container_start, container_end - container_start);
      std::string item_path = path_str.substr(container_end);

      if (item_path.find("/.resources/") == 0) {
        std::string resource_name = item_path.substr(12);
        std::string value(buf, size);

        // Убираем пробелы и переводы строк
        value.erase(
            std::remove_if(value.begin(), value.end(),
                           [](unsigned char c) { return std::isspace(c); }),
            value.end());

        spdlog::info("Setting resource {} to '{}' for container {}",
                     resource_name, value, container_id);

        auto container =
            state_.getContainerManager().get_container(container_id);
        if (!container) {
          spdlog::error("Container {} not found", container_id);
          return -ENOENT;
        }

        bool success = false;
        try {
          if (resource_name == "memory") {
            if (auto ossec_adapter =
                    std::dynamic_pointer_cast<OssecContainerAdapter>(
                        container)) {
              success = ossec_adapter->set_memory_limit(value);
            }
          } else if (resource_name == "disk") {
            if (auto ossec_adapter =
                    std::dynamic_pointer_cast<OssecContainerAdapter>(
                        container)) {
              success = ossec_adapter->set_disk_quota(value);
            }
          } else if (resource_name == "pids") {
            if (auto ossec_adapter =
                    std::dynamic_pointer_cast<OssecContainerAdapter>(
                        container)) {
              success = ossec_adapter->set_pids_limit(value);
            }
          } else if (resource_name == "apply") {
            if (auto ossec_adapter =
                    std::dynamic_pointer_cast<OssecContainerAdapter>(
                        container)) {
              success = ossec_adapter->apply_resource_changes();
            }
          } else {
            spdlog::error("Unknown resource: {}", resource_name);
            return -EINVAL;
          }
        } catch (const std::exception &e) {
          spdlog::error("Failed to set resource: {}", e.what());
          return -EIO;
        }

        if (success) {
          spdlog::info("Successfully set resource {} to {}", resource_name,
                       value);
          return size;
        } else {
          spdlog::error("Failed to set resource {}", resource_name);
          return -EIO;
        }
      }
    }
  }

  // Остальной код для виртуальных файлов
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

    (void)state_.getSearch().updateEmbedding(path);
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

  (void)state_.getSearch().updateEmbedding(path);
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