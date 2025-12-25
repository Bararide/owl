#ifndef OWL_VFS_FS_HANDLER_READDIR
#define OWL_VFS_FS_HANDLER_READDIR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Readdir final : public Handler<Readdir> {
  int operator()(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags) const {
    spdlog::info("Readdir handler called for path: {}", path);

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_read(path, buf, filler, offset, fi, flags);
    }

    return handle_virtual_file_read(path, buf, filler, offset, fi, flags);
  }

private:
  int handle_container_read(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) const {
    spdlog::info("Container Readdir: {}", path);
    return 0;
  }

  int handle_virtual_file_read(const char *path, void *buf,
                               fuse_fill_dir_t filler, off_t offset,
                               struct fuse_file_info *fi,
                               enum fuse_readdir_flags flags) const {
    spdlog::info("Virtual file Readdir: {}", path);
    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_READDIR