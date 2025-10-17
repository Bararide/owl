#include "vectorfs.hpp"

namespace owl::vectorfs {

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
    return container_manager_.get_container(container_id);
  }
  return nullptr;
}

std::string VectorFS::generate_container_listing() {
  std::stringstream ss;
  ss << "=== Knowledge Containers ===\n\n";
  auto containers = container_manager_.get_all_containers();
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
  ss << "Available: " << container_manager_.get_available_container_count()
     << " containers\n";
  return ss.str();
}

std::string
VectorFS::generate_container_content(const std::string &container_id) {
  auto container = container_manager_.get_container(container_id);
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

  auto files = container->list_files("/");
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
  auto container = container_manager_.get_container(container_id);
  if (!container) {
    return "Container not found: " + container_id;
  }

  std::stringstream ss;
  ss << "=== Search in Container: " << container_id << " ===\n\n";
  ss << "Query: " << query << "\n\n";

  ss << "Search functionality for containers is under development.\n";
  ss << "Available files:\n";

  auto files = container->list_files("/");
  for (const auto &file : files) {
    ss << "  - " << file << "\n";
  }

  return ss.str();
}

void VectorFS::updateFromSharedMemory() {
  if (!shm_manager || !shm_manager->initialize()) {
    spdlog::warn("Failed to initialize shared memory in FUSE");
    return;
  }

  if (!shm_manager->needsUpdate()) {
    return;
  }

  spdlog::info("Updating from shared memory with compression...");

  int file_count = shm_manager->getFileCount();
  for (int i = 0; i < file_count; i++) {
    const auto *shared_info = shm_manager->getFile(i);
    if (shared_info) {
      std::string path(shared_info->path);

      if (virtual_files.count(path) == 0) {
        fileinfo::FileInfo fi;
        fi.mode = shared_info->mode;
        fi.size = shared_info->size;

        std::vector<uint8_t> compressed_content(
            shared_info->content, shared_info->content + shared_info->size);
        auto decompressed_content = decompress_data(compressed_content);
        fi.content = std::string(decompressed_content.begin(),
                                 decompressed_content.end());

        fi.uid = shared_info->uid;
        fi.gid = shared_info->gid;
        fi.access_time = shared_info->access_time;
        fi.modification_time = shared_info->modification_time;
        fi.create_time = shared_info->create_time;

        virtual_files[path] = fi;
        spdlog::info(
            "Added compressed file from shared memory: {} ({} -> {} bytes)",
            path, shared_info->size, fi.content.size());

        update_embedding(path.c_str());
        index_needs_rebuild = true;
      }
    }
  }

  shm_manager->clearUpdateFlag();
  spdlog::info("Shared memory update with compression completed");
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

  if (strcmp(path, "/.containers") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    return 0;
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

  if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end == std::string::npos) {
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 2;
    } else {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 1024;
    }
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);
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
    stbuf->st_size = fi.size;
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
      stbuf->st_size = fi.size;
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

    if (shm_manager && shm_manager->initialize()) {
      if (shm_manager->needsUpdate()) {
        updateFromSharedMemory();
        shm_manager->clearUpdateFlag();
      }

      int file_count = shm_manager->getFileCount();
      for (int i = 0; i < file_count; i++) {
        const auto *shared_info = shm_manager->getFile(i);
        if (shared_info) {
          std::string file_path(shared_info->path);
          std::string file_name =
              std::filesystem::path(file_path).filename().string();
          if (!file_name.empty()) {
            filler(buf, file_name.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
          }
        }
      }
    }

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

    auto containers = container_manager_.get_all_containers();
    for (const auto &container : containers) {
      filler(buf, container->get_id().c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

  } else if (strcmp(path, "/.containers/.search") == 0) {
    auto containers = container_manager_.get_all_containers();
    for (const auto &container : containers) {
      filler(buf, container->get_id().c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

  } else if (strncmp(path, "/.containers/", 13) == 0) {
    std::string path_str(path);
    size_t container_start = 13;
    size_t container_end = path_str.find('/', container_start);

    if (container_end == std::string::npos) {
      std::string container_id = path_str.substr(container_start);
      auto container = container_manager_.get_container(container_id);
      if (container) {
        auto files = container->list_files("/");
        for (const auto &file : files) {
          filler(buf, file.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        }
      }
    }
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
    ss << "Total containers: " << container_manager_.get_container_count()
       << "\n";
    ss << "Available containers: "
       << container_manager_.get_available_container_count() << "\n";

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

    if (shm_manager && shm_manager->initialize()) {
      int file_count = shm_manager->getFileCount();
      for (int i = 0; i < file_count; i++) {
        const auto *shared_info = shm_manager->getFile(i);
        if (shared_info) {
          ss << "SHM: " << shared_info->path << " (" << shared_info->size
             << " bytes)\n";
        }
      }
    }

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
    index_needs_rebuild = true;
    rebuild_index();

    std::stringstream ss;
    ss << "Forcing reindex...\n";
    ss << "Reindex completed!\n";
    ss << "Indexed files: " << index_to_path.size() << "\n";

    std::string content = ss.str();
    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.markov") == 0) {
    test_markov_chains();
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
      index_needs_rebuild = true;
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

    update_embedding(path);
    index_needs_rebuild = true;

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

  update_embedding(path);
  index_needs_rebuild = true;

  return size;
}

int VectorFS::unlink(const char *path) {
  if (virtual_files.erase(path) == 0) {
    return -ENOENT;
  }
  index_needs_rebuild = true;
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
