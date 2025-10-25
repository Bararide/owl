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

struct SharedContainerInfo {
  char container_id[256];
  char owner[256];
  char namespace_[256];
  char status[64];
  size_t size;
  bool available;
  char labels[1024];
  char commands[1024];

  SharedContainerInfo() : size(0), available(false) {
    container_id[0] = '\0';
    owner[0] = '\0';
    namespace_[0] = '\0';
    status[0] = '\0';
    labels[0] = '\0';
    commands[0] = '\0';
  }
};

struct SharedMemoryData {
  std::mutex mutex;
  int file_count;
  SharedFileInfo files[100];
  int container_count;
  SharedContainerInfo containers[50];
  bool needs_update;
  bool containers_updated;

  SharedMemoryData()
      : file_count(0), container_count(0), needs_update(false),
        containers_updated(false) {}
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

  bool addContainer(const std::string &container_id, const std::string &owner,
                    const std::string &namespace_, const std::string &status,
                    size_t size, bool available, const std::string &labels_json,
                    const std::string &commands_json) {
    std::lock_guard<std::mutex> lock(data->mutex);

    if (data->container_count >= 50) {
      spdlog::error("Shared memory full, cannot add more containers");
      return false;
    }

    for (int i = 0; i < data->container_count; i++) {
      if (strcmp(data->containers[i].container_id, container_id.c_str()) == 0) {
        spdlog::warn("Container already exists in shared memory: {}",
                     container_id);
        return true;
      }
    }

    SharedContainerInfo container_info;
    strncpy(container_info.container_id, container_id.c_str(),
            sizeof(container_info.container_id) - 1);
    container_info.container_id[sizeof(container_info.container_id) - 1] = '\0';

    strncpy(container_info.owner, owner.c_str(),
            sizeof(container_info.owner) - 1);
    container_info.owner[sizeof(container_info.owner) - 1] = '\0';

    strncpy(container_info.namespace_, namespace_.c_str(),
            sizeof(container_info.namespace_) - 1);
    container_info.namespace_[sizeof(container_info.namespace_) - 1] = '\0';

    strncpy(container_info.status, status.c_str(),
            sizeof(container_info.status) - 1);
    container_info.status[sizeof(container_info.status) - 1] = '\0';

    container_info.size = size;
    container_info.available = available;

    strncpy(container_info.labels, labels_json.c_str(),
            sizeof(container_info.labels) - 1);
    container_info.labels[sizeof(container_info.labels) - 1] = '\0';

    strncpy(container_info.commands, commands_json.c_str(),
            sizeof(container_info.commands) - 1);
    container_info.commands[sizeof(container_info.commands) - 1] = '\0';

    data->containers[data->container_count] = container_info;
    data->container_count++;
    data->containers_updated = true;
    data->needs_update = true;

    spdlog::info("Added container to shared memory: {} (owner: {}, status: {})",
                 container_id, owner, status);
    return true;
  }

  bool removeContainer(const std::string &container_id) {
    std::lock_guard<std::mutex> lock(data->mutex);

    for (int i = 0; i < data->container_count; i++) {
      if (strcmp(data->containers[i].container_id, container_id.c_str()) == 0) {
        for (int j = i; j < data->container_count - 1; j++) {
          data->containers[j] = data->containers[j + 1];
        }
        data->container_count--;
        data->containers_updated = true;
        data->needs_update = true;

        spdlog::info("Removed container from shared memory: {}", container_id);
        return true;
      }
    }

    spdlog::warn("Container not found in shared memory: {}", container_id);
    return false;
  }

  bool needsUpdate() const { return data->needs_update; }
  bool containersNeedUpdate() const { return data->containers_updated; }

  void clearUpdateFlag() {
    std::lock_guard<std::mutex> lock(data->mutex);
    data->needs_update = false;
    data->containers_updated = false;
  }

  int getFileCount() const { return data->file_count; }
  int getContainerCount() const { return data->container_count; }

  const SharedFileInfo *getFile(int index) const {
    if (index < 0 || index >= data->file_count) {
      return nullptr;
    }
    return &data->files[index];
  }

  const SharedContainerInfo *getContainer(int index) const {
    if (index < 0 || index >= data->container_count) {
      return nullptr;
    }
    return &data->containers[index];
  }

  const SharedContainerInfo *
  findContainer(const std::string &container_id) const {
    for (int i = 0; i < data->container_count; i++) {
      if (strcmp(data->containers[i].container_id, container_id.c_str()) == 0) {
        return &data->containers[i];
      }
    }
    return nullptr;
  }

  void clearFiles() {
    std::lock_guard<std::mutex> lock(data->mutex);
    data->file_count = 0;
    data->needs_update = true;
  }

  void clearContainers() {
    std::lock_guard<std::mutex> lock(data->mutex);
    data->container_count = 0;
    data->containers_updated = true;
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