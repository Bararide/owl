#ifndef SHARED_TYPES_HPP
#define SHARED_TYPES_HPP

#include <string>
#include <vector>

namespace vfs::ipc {

struct FileCreateRequest {
  std::string path;
  std::string content;
  mode_t mode;
};

struct FileUpdateRequest {
  std::string path;
  std::string content;
  off_t offset;
};

struct FileDeleteRequest {
  std::string path;
};

struct SyncResponse {
  bool success;
  std::string message;
};

} // namespace vfs::ipc

#endif // SHARED_TYPES_HPP
