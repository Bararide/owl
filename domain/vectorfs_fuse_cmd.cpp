#include "vectorfs.hpp"

namespace vfs::vectorfs {
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

int VectorFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
  if (strcmp(path, "/") == 0) {
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".search", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".reindex", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".embeddings", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".markov", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".all", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".debug", nullptr, 0, FUSE_FILL_DIR_PLUS);

    for (const auto &dir : virtual_dirs) {
      if (dir != "/") {
        const char *dir_name = dir.c_str();
        if (dir_name[0] == '/') {
          dir_name++;
        }
        filler(buf, dir_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
      }
    }

    for (const auto &file : virtual_files) {
      const char *file_name = file.first.c_str();
      if (file_name[0] == '/') {
        file_name++;
      }
      filler(buf, file_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
    }
  } else if (strcmp(path, "/.search") == 0) {
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
  } else {
    return -ENOENT;
  }
  return 0;
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
  if (strncmp(path, "/.search/", 9) == 0) {
    return 0;
  }

  if (strncmp(path, "/.markov", 9) == 0) {
    return 0;
  }

  if (strcmp(path, "/.reindex") == 0 || strcmp(path, "/.embeddings") == 0 ||
      strcmp(path, "/.all") == 0 || strcmp(path, "/.debug") == 0) {
    return 0;
  }

  if (virtual_files.count(path) == 0) {
    return -ENOENT;
  }

  return 0;
}

int VectorFS::read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  record_file_access(path, "read");

  if (strcmp(path, "/.debug") == 0) {
    std::stringstream ss;
    ss << "=== DEBUG INFO ===\n";
    ss << "Total virtual_files: " << virtual_files.size() << "\n";
    ss << "Files:\n";
    for (const auto &file : virtual_files) {
      ss << "  - " << file.first << " (" << file.second.size << " bytes)\n";
    }
    ss << "Total virtual_dirs: " << virtual_dirs.size() << "\n";
    ss << "Dirs:\n";
    for (const auto &dir : virtual_dirs) {
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

    std::string content = generate_enhanced_search_result(query);

    if (offset >= content.size()) {
      return 0;
    }
    size_t len = std::min(content.size() - offset, size);
    memcpy(buf, content.c_str() + offset, len);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Enhanced search for '{}' took {} ms", query, duration);
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
    for (const auto &[file_path, _] : virtual_files) {
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

int VectorFS::create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  if (virtual_files.count(path) > 0 || virtual_dirs.count(path) > 0) {
    spdlog::error("File already exists: {}", path);
    return -EEXIST;
  }

  std::string parent_dir = path;
  fprintf(stderr, "Creating file: %s\n", path);
  size_t last_slash = parent_dir.find_last_of('/');
  if (last_slash != std::string::npos) {
    parent_dir = parent_dir.substr(0, last_slash);
    if (parent_dir.empty())
      parent_dir = "/";
    if (virtual_dirs.count(parent_dir) == 0)
      return -ENOENT;
  }

  fprintf(stderr, "Parent dir: %s\n", parent_dir.c_str());

  std::string current_path = parent_dir;
  while (!current_path.empty() && current_path != "/") {
    if (virtual_dirs.count(current_path) == 0) {
      virtual_dirs.insert(current_path);
    }

    size_t pos = current_path.find_last_of('/');
    if (pos == std::string::npos) {
      break;
    }
    current_path = current_path.substr(0, pos);
    if (current_path.empty()) {
      current_path = "/";
    }
  }

  fprintf(stderr, "Created parent dirs %s\n", current_path.c_str());

  time_t now = time(nullptr);
  virtual_files[path] =
      fileinfo::FileInfo(S_IFREG | 0644, 0,
                         "", getuid(), getgid(), now, now, now);

  fprintf(stderr, "Created file %s\n", path);

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
  auto it = virtual_files.find(path);
  if (it == virtual_files.end())
    return -ENOENT;
  if ((it->second.mode & S_IWUSR) == 0)
    return -EACCES;

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
} // namespace vfs::vectorfs
