#ifndef OWL_VFS_FS_HANDLER_WRITE
#define OWL_VFS_FS_HANDLER_WRITE

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Write final : public Handler<Write> {
  int operator()(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) const {
    spdlog::info("Write handler called for path: {}", path);

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_write(path, buf, size, offset, fi);
    }

    return handle_virtual_file_write(path, buf, size, offset, fi);
  }

private:
  int handle_container_write(const char *path, const char *buf, size_t size,
                             off_t offset, struct fuse_file_info *fi) const {
    spdlog::info("Container write: {}", path);
    return size;
  }

  int handle_virtual_file_write(const char *path, const char *buf, size_t size,
                                off_t offset, struct fuse_file_info *fi) const {
    spdlog::info("Virtual file write: {}", path);
    return size;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_WRITE