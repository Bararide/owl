#ifndef OWL_VFS_FS_HANDLER_CREATE
#define OWL_VFS_FS_HANDLER_CREATE

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Create final : public Handler<Create> {
  int operator()(const char *path, mode_t mode,
                 struct fuse_file_info *fi) const {
    spdlog::info("Create handler called for path: {}", path);

    return handle_virtual_file_write(path, mode, fi);
  }

private:
  //   int handle_container_write(const char *path, const char *buf, size_t
  //   size,
  //                              off_t offset, struct fuse_file_info *fi) const
  //                              {
  //     spdlog::info("Container write: {}", path);
  //     return size;
  //   }

  int handle_virtual_file_write(const char *path, mode_t mode,
                                struct fuse_file_info *fi) const {
    spdlog::info("Virtual file write: {}", path);
    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_CREATE