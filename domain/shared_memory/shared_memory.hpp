#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

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

struct SharedFileInfo {
  char path[1024];
  char content[32768];
  size_t size;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  time_t access_time;
  time_t modification_time;
  time_t create_time;

  SharedFileInfo()
      : size(0), mode(0), uid(0), gid(0), access_time(0), modification_time(0),
        create_time(0) {
    path[0] = '\0';
    content[0] = '\0';
  }
};

struct SharedMemoryData {
  std::mutex mutex;
  int file_count;
  SharedFileInfo files[100];
  bool needs_update;

  SharedMemoryData() : file_count(0), needs_update(false) {}
};

class SharedMemoryManager {
public:
  SharedMemoryManager() : shm_fd(-1), data(nullptr) {}

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
      return false;
    }

    data = static_cast<SharedMemoryData *>(mmap(NULL, sizeof(SharedMemoryData),
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED, shm_fd, 0));

    if (data == MAP_FAILED) {
      spdlog::error("mmap failed: {}", strerror(errno));
      return false;
    }

    spdlog::info("Shared memory initialized successfully");
    return true;
  }
  
  bool addFile(const std::string &path, const std::string &content) {
    std::lock_guard<std::mutex> lock(data->mutex);

    if (data->file_count >= 100) {
      spdlog::error("Shared memory full, cannot add more files");
      return false;
    }

    SharedFileInfo file_info;
    strncpy(file_info.path, path.c_str(), sizeof(file_info.path) - 1);
    file_info.path[sizeof(file_info.path) - 1] = '\0';

    size_t copy_size = std::min(content.size(), sizeof(file_info.content) - 1);
    strncpy(file_info.content, content.c_str(), copy_size);
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

  bool needsUpdate() const { return data->needs_update; }

  void clearUpdateFlag() {
    std::lock_guard<std::mutex> lock(data->mutex);
    data->needs_update = false;
  }

  int getFileCount() const { return data->file_count; }

  const SharedFileInfo *getFile(int index) const {
    if (index < 0 || index >= data->file_count) {
      return nullptr;
    }
    return &data->files[index];
  }

  void clearFiles() {
    std::lock_guard<std::mutex> lock(data->mutex);
    data->file_count = 0;
    data->needs_update = true;
  }

  ~SharedMemoryManager() {
    if (data != MAP_FAILED && data != nullptr) {
      munmap(data, sizeof(SharedMemoryData));
    }
    if (shm_fd != -1) {
      close(shm_fd);
      shm_unlink("/vectorfs_shm");
    }
  }

private:
  int shm_fd;
  SharedMemoryData *data;
};

} // namespace owl::shared

#endif // SHARED_MEMORY_HPP