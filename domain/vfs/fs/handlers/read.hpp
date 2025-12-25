#ifndef OWL_VFS_FS_HANDLER_READ
#define OWL_VFS_FS_HANDLER_READ

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Read final : public Handler<Read> {
  int operator()(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) const {
    spdlog::info("Read handler called for path: {}", path);

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_read(path, buf, size, offset, fi);
    }

    return handle_virtual_file_read(path, buf, size, offset, fi);
  }

private:
  int handle_container_read(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi) const {
    spdlog::info("Container Read: {}", path);
    return size;
  }

  int handle_virtual_file_read(const char *path, const char *buf, size_t size,
                               off_t offset, struct fuse_file_info *fi) const {
    spdlog::info("Virtual file Read: {}", path);
    return size;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_READ