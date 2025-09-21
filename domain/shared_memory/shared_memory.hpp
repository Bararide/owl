#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

#include <array>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace owl::shared {

constexpr size_t MAX_PATH_LENGTH = 1024;
constexpr size_t MAX_CONTENT_SIZE = 32768;
constexpr size_t MAX_FILES = 100;

struct SharedFileInfo {
  std::array<char, MAX_PATH_LENGTH> path;
  std::array<char, MAX_CONTENT_SIZE> content;
  size_t size;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  time_t access_time;
  time_t modification_time;
  time_t create_time;

  SharedFileInfo() noexcept
      : size(0), mode(0), uid(0), gid(0), access_time(0), modification_time(0),
        create_time(0) {
    path[0] = '\0';
    content[0] = '\0';
  }
};

struct SharedMemoryData {
  std::mutex mutex;
  int file_count;
  std::array<SharedFileInfo, MAX_FILES> files;
  bool needs_update;

  SharedMemoryData() noexcept : file_count(0), needs_update(false) {}
};

class SharedMemoryManager {
public:
  SharedMemoryManager() noexcept : shm_fd(-1), data(nullptr) {}

  SharedMemoryManager(const SharedMemoryManager &) = delete;
  SharedMemoryManager &operator=(const SharedMemoryManager &) = delete;

  static SharedMemoryManager &getInstance() {
    static SharedMemoryManager instance;
    return instance;
  }

  bool initialize() {
    shm_fd = shm_open("/vectorfs_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      spdlog::error("shm_open failed: {}", strerror(errno));
      return false;
    }

    if (ftruncate(shm_fd, sizeof(SharedMemoryData)) == -1) {
      spdlog::error("ftruncate failed: {}", strerror(errno));
      close(shm_fd);
      return false;
    }

    data = static_cast<SharedMemoryData *>(
        mmap(nullptr, sizeof(SharedMemoryData), PROT_READ | PROT_WRITE,
             MAP_SHARED, shm_fd, 0));

    if (data == MAP_FAILED) {
      spdlog::error("mmap failed: {}", strerror(errno));
      close(shm_fd);
      return false;
    }

    new (data) SharedMemoryData();

    spdlog::info("Shared memory initialized successfully");
    return true;
  }

  bool addFile(const std::string &path, const std::string &content) {
    std::lock_guard<std::mutex> lock(data->mutex);

    if (data->file_count >= MAX_FILES) {
      spdlog::error("Shared memory full, cannot add more files");
      return false;
    }

    SharedFileInfo file_info;

    if (path.size() >= MAX_PATH_LENGTH) {
      spdlog::warn("Path too long, truncating: {}", path);
    }
    strncpy(file_info.path.data(), path.c_str(), MAX_PATH_LENGTH - 1);
    file_info.path[MAX_PATH_LENGTH - 1] = '\0';

    size_t copy_size = std::min(content.size(), MAX_CONTENT_SIZE - 1);
    strncpy(file_info.content.data(), content.c_str(), copy_size);
    file_info.content[copy_size] = '\0';

    file_info.size = copy_size;
    file_info.mode = S_IFREG | 0644;
    file_info.uid = getuid();
    file_info.gid = getgid();

    time_t now = time(nullptr);
    file_info.access_time = now;
    file_info.modification_time = now;
    file_info.create_time = now;

    data->files[data->file_count] = file_info;
    data->file_count++;
    data->needs_update = true;

    spdlog::info("Added file to shared memory: {} ({} bytes)", path, copy_size);
    return true;
  }

  bool needsUpdate() const noexcept {
    return data ? data->needs_update : false;
  }

  void clearUpdateFlag() noexcept {
    if (data) {
      std::lock_guard<std::mutex> lock(data->mutex);
      data->needs_update = false;
    }
  }

  int getFileCount() const noexcept { return data ? data->file_count : 0; }

  const SharedFileInfo *getFile(int index) const noexcept {
    if (!data || index < 0 || index >= data->file_count) {
      return nullptr;
    }
    return &data->files[index];
  }

  void clearFiles() noexcept {
    if (data) {
      std::lock_guard<std::mutex> lock(data->mutex);
      data->file_count = 0;
      data->needs_update = true;
    }
  }

  ~SharedMemoryManager() { cleanup(); }

private:
  void cleanup() noexcept {
    if (data != MAP_FAILED && data != nullptr) {
      munmap(data, sizeof(SharedMemoryData));
      data = nullptr;
    }

    if (shm_fd != -1) {
      close(shm_fd);
      shm_unlink("/vectorfs_shm");
      shm_fd = -1;
    }
  }

private:
  int shm_fd;
  SharedMemoryData *data;
};

} // namespace owl::shared

#endif // SHARED_MEMORY_HPP