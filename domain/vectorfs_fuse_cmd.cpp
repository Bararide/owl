#include "vectorfs.hpp"

namespace owl::vectorfs {
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

  if (strcmp(path, "/.debug") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1024;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    return 0;
  }

  if (strcmp(path, "/.markov") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 2048;
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

  if (strcmp(path, "/.all") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 2048;
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

  if (virtual_dirs_.count(path)) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    time_t now = time(nullptr);
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = now;
    return 0;
  }

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    time_t now = time(nullptr);
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = now;
    return 0;
  }

  auto it = virtual_files_.find(path);
  if (it != virtual_files_.end()) {
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

// void VectorFS::updateFromSharedMemory() {
//   if (!shm_manager_ || !shm_manager_->initialize()) {
//     spdlog::warn("Failed to initialize shared memory in FUSE");
//     return;
//   }

//   if (!shm_manager_->needsUpdate()) {
//     return;
//   }

//   spdlog::info("Updating from shared memory...");

//   int file_count = shm_manager_->getFileCount();
//   for (int i = 0; i < file_count; i++) {
//     const auto *shared_info = shm_manager_->getFile(i);
//     if (shared_info) {
//       std::string path(shared_info->path.data());

//       if (virtual_files_.count(path) == 0) {
//         fileinfo::FileInfo fi;
//         fi.mode = shared_info->mode;
//         fi.size = shared_info->size;
//         fi.content = std::string(shared_info->content.data(), shared_info->size);
//         fi.uid = shared_info->uid;
//         fi.gid = shared_info->gid;
//         fi.access_time = shared_info->access_time;
//         fi.modification_time = shared_info->modification_time;
//         fi.create_time = shared_info->create_time;

//         virtual_files_[path] = fi;
//         spdlog::info("Added file from shared memory: {} ({} bytes)", path,
//                      shared_info->size);

//         update_embedding(path.c_str());
//         index_needs_rebuild_ = true;
//       }
//     }
//   }

//   shm_manager_->clearUpdateFlag();
//   spdlog::info("Shared memory update completed");
// }

void VectorFS::updateFromSharedMemory() {
  if (!shm_manager_ || !shm_manager_->initialize()) {
    spdlog::warn("Failed to initialize shared memory in FUSE");
    return;
  }

  if (!shm_manager_->needsUpdate()) {
    return;
  }

  spdlog::info("Updating from shared memory...");

  int file_count = shm_manager_->getFileCount();
  for (int i = 0; i < file_count; i++) {
    const auto *shared_info = shm_manager_->getFile(i);
    if (shared_info) {
      std::string path(shared_info->path.data());

      std::vector<uint8_t> shared_content(shared_info->content.begin(),
                                          shared_info->content.end());

      auto decompressed = decompress_data(shared_content, shared_info->size);

      if (virtual_files_.count(path) == 0) {
        fileinfo::FileInfo fi;
        fi.mode = shared_info->mode;
        fi.original_size = shared_info->size;
        fi.size = decompressed.size();
        fi.content = decompressed;
        fi.uid = shared_info->uid;
        fi.gid = shared_info->gid;
        fi.access_time = shared_info->access_time;
        fi.modification_time = shared_info->modification_time;
        fi.create_time = shared_info->create_time;

        virtual_files_[path] = fi;
        spdlog::info("Added file from shared memory: {} ({} bytes)", path,
                     shared_info->size);

        update_embedding(path.c_str());
        index_needs_rebuild_ = true;
      }
    }
  }

  shm_manager_->clearUpdateFlag();
  spdlog::info("Shared memory update completed");
}

int VectorFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    
    // Всегда добавляем . и .. в начало
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    if (strcmp(path, "/") == 0) {
        // Корневая директория
        filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".reindex", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".embeddings", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".markov", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);

        if (shm_manager_ && shm_manager_->initialize()) {
            if (shm_manager_->needsUpdate()) {
                updateFromSharedMemory();
                shm_manager_->clearUpdateFlag();
            }

            int file_count = shm_manager_->getFileCount();
            for (int i = 0; i < file_count; i++) {
                const auto *shared_info = shm_manager_->getFile(i);
                if (shared_info) {
                    std::string file_path(shared_info->path.data());
                    const char *file_name = file_path.c_str();
                    if (file_name[0] == '/') {
                        file_name++;
                    }
                    filler(buf, file_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
                }
            }
        }

        for (const auto &dir : virtual_dirs_) {
            if (dir != "/") {
                const char *dir_name = dir.c_str();
                if (dir_name[0] == '/') {
                    dir_name++;
                }
                filler(buf, dir_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
            }
        }

        for (const auto &file : virtual_files_) {
            const char *file_name = file.first.c_str();
            if (file_name[0] == '/') {
                file_name++;
            }
            filler(buf, file_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
        }
        
    } else if (strcmp(path, "/.search") == 0) {
        filler(buf, "example_query", nullptr, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "help.txt", nullptr, 0, FUSE_FILL_DIR_PLUS);
        
    } else if (virtual_dirs_.count(path) > 0) {
        std::string dir_path = path;
        if (dir_path.back() != '/') {
            dir_path += "/";
        }
        
        for (const auto &file : virtual_files_) {
            if (file.first.find(dir_path) == 0) {
                std::string relative_path = file.first.substr(dir_path.length());
                if (relative_path.find('/') == std::string::npos) {
                    filler(buf, relative_path.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
                }
            }
        }
        
    } else {
        return -ENOENT;
    }

    return 0;
}

int VectorFS::rmdir(const char *path) {
  if (virtual_dirs_.count(path) == 0) {
    return -ENOENT;
  }

  std::string dir = path;
  if (dir == "/")
    dir = "";

  for (auto it = virtual_files_.begin(); it != virtual_files_.end();) {
    if (it->first.find(dir) == 0) {
      it = virtual_files_.erase(it);
      index_needs_rebuild_ = true;
    } else {
      ++it;
    }
  }

  virtual_dirs_.erase(path);
  return 0;
}

int VectorFS::mkdir(const char *path, mode_t mode) {
  if (virtual_dirs_.count(path) > 0 || virtual_files_.count(path) > 0) {
    return -EEXIST;
  }

  std::string parent_dir = path;
  size_t last_slash = parent_dir.find_last_of('/');
  if (last_slash != std::string::npos) {
    parent_dir = parent_dir.substr(0, last_slash);
    if (parent_dir.empty()) {
      parent_dir = "/";
    }
    if (virtual_dirs_.count(parent_dir) == 0) {
      return -ENOENT;
    }
  }

  virtual_dirs_.insert(path);
  return 0;
}

int VectorFS::open(const char *path, struct fuse_file_info *fi) {
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

  auto it = virtual_files_.find(path);
  if (it == virtual_files_.end()) {
    return -ENOENT;
  }

  fi->fh = reinterpret_cast<uint64_t>(&it->second);
  return 0;
}

int VectorFS::read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  record_file_access(path, "read");

  if (shm_manager_->initialize() && shm_manager_->needsUpdate()) {
    updateFromSharedMemory();
    shm_manager_->clearUpdateFlag();
  }

  if (strcmp(path, "/.debug") == 0) {
    std::stringstream ss;
    ss << "=== DEBUG INFO ===\n";
    ss << "Total virtual_files_: " << virtual_files_.size() << "\n";
    ss << "Files:\n";

    ss << "Process PID: " << getpid() << "\n";
    ss << "Parent PID: " << getppid() << "\n";

    for (const auto &file : virtual_files_) {
      ss << file.first << " (" << file.second.size << " bytes)\n";
      ss << "  Create: " << std::ctime(&file.second.create_time);
      ss << "  Access: " << std::ctime(&file.second.access_time);
      ss << "  Modify: " << std::ctime(&file.second.modification_time);
    }

    ss << "Total virtual_dirs_: " << virtual_dirs_.size() << "\n";
    ss << "Dirs:\n";
    for (const auto &dir : virtual_dirs_) {
      ss << "  - " << dir << "\n";
    }

    std::string content = ss.str();
    if (offset >= content.size())
      return 0;
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strncmp(path, "/.search/", 9) == 0) {
    auto start = std::chrono::high_resolution_clock::now();
    std::string query = path + 9;

    query = url_decode(query);
    std::replace(query.begin(), query.end(), '_', ' ');

    auto it = virtual_files_.find(query);

    std::string content;
    if (it->second.is_compressed) {
      content = get_file_content_decompressed(path);
    } else {
      content = it->second.content;
    }

    if (offset >= content.size()) {
      return 0;
    }

    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.reindex") == 0) {
    std::stringstream ss;
    ss << "Forcing reindex...\n";
    index_needs_rebuild_ = true;
    rebuild_index();
    ss << "Reindex completed!\n";
    ss << "Indexed files: " << index_to_path_.size() << "\n";

    std::string content = ss.str();
    if (offset >= content.size()) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  if (strcmp(path, "/.markov") == 0) {
    test_markov_chains();
    std::string content = generate_markov_test_result();

    if (offset >= content.size()) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }
  if (strcmp(path, "/.all") == 0) {
    std::stringstream ss;
    for (const auto &[file_path, _] : virtual_files_) {
      ss << "--- " << file_path << " ---\n";
    }

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
    ss << "Total files: " << virtual_files_.size() << "\n";
    ss << "Files with embeddings: ";

    int count = 0;
    for (const auto &[file_path, file_info] : virtual_files_) {
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

    std::string content = ss.str();
    if (offset >= content.size()) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);
    return len;
  }

  auto it = virtual_files_.find(path);
  if (it == virtual_files_.end()) {
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

int VectorFS::create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  if (virtual_files_.count(path) > 0 || virtual_dirs_.count(path) > 0) {
    spdlog::error("File already exists: {}", path);
    return -EEXIST;
  }

  time_t now = time(nullptr);
  auto &file_info = virtual_files_[path];
  file_info = fileinfo::FileInfo(S_IFREG | 0644, 0, 0, "", getuid(), getgid(),
                                 now, now, now);
  file_info.is_compressed = true;

  fi->fh = reinterpret_cast<uint64_t>(&file_info);

  return 0;
}

int VectorFS::utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi) {
  auto it = virtual_files_.find(path);
  if (it == virtual_files_.end()) {
    return -ENOENT;
  }

  it->second.access_time = tv[0].tv_sec;
  it->second.modification_time = tv[1].tv_sec;

  return 0;
}

int VectorFS::write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
  if (fi->fh == 0) {
    auto it = virtual_files_.find(path);
    if (it == virtual_files_.end()) {
      return -ENOENT;
    }
    if ((it->second.mode & S_IWUSR) == 0) {
      return -EACCES;
    }

    std::string current_content;
    if (it->second.is_compressed) {
      current_content = get_file_content_decompressed(path);
    } else {
      current_content = it->second.content;
    }

    if (offset + size > current_content.size()) {
      current_content.resize(offset + size);
    }

    memcpy(&current_content[offset], buf, size);

    update_file_content_compressed(path, current_content);

    return size;
  }

  auto *file_info = reinterpret_cast<fileinfo::FileInfo *>(fi->fh);

  if ((file_info->mode & S_IWUSR) == 0) {
    return -EACCES;
  }

  std::string current_content;
  if (file_info->is_compressed) {
    std::vector<uint8_t> compressed_data(file_info->content.begin(),
                                         file_info->content.end());
    current_content =
        decompress_data(compressed_data, file_info->original_size);
  } else {
    current_content = file_info->content;
  }

  if (offset + size > current_content.size()) {
    current_content.resize(offset + size);
  }

  memcpy(&current_content[offset], buf, size);

  auto compressed_content = compress_data(current_content);
  file_info->content =
      std::string(compressed_content.begin(), compressed_content.end());
  file_info->size = compressed_content.size();
  file_info->original_size = current_content.size();
  file_info->is_compressed = true;
  file_info->modification_time = time(nullptr);
  file_info->access_time = file_info->modification_time;

  update_embedding(path);
  index_needs_rebuild_ = true;

  return size;
}

int VectorFS::unlink(const char *path) {
  if (virtual_files_.erase(path) == 0) {
    return -ENOENT;
  }
  index_needs_rebuild_ = true;
  return 0;
}

int VectorFS::setxattr(const char *path, const char *name, const char *value,
                       size_t size, int flags) {
  return -ENOTSUP;
}

int VectorFS::getxattr(const char *path, const char *name, char *value,
                       size_t size) {
  auto it = virtual_files_.find(path);
  if (it == virtual_files_.end())
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
