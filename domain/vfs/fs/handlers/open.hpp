#ifndef OWL_VFS_FS_HANDLER_OPEN
#define OWL_VFS_FS_HANDLER_OPEN

#include "handler.hpp"

namespace owl {

struct Open final : public Handler<Open> {
  int operator()(const char *path, struct fuse_file_info *fi) const {
    spdlog::info("Open handler called for path: {}", path);

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_write(path, fi);
    }

    return handle_virtual_file_write(path, fi);
  }

private:
  int handle_container_write(const char *path, struct fuse_file_info *fi) const {
    spdlog::info("Container Open: {}", path);
    return 0;
  }

  int handle_virtual_file_write(const char *path, struct fuse_file_info *fi) const {
    spdlog::info("Virtual file Open: {}", path);
    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_OPEN