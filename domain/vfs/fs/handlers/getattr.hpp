#ifndef OWL_VFS_FS_HANDLER_GETATTR
#define OWL_VFS_FS_HANDLER_GETATTR

#include "handler.hpp"
#include <fuse3/fuse.h>
#include <sys/stat.h>

namespace owl {

struct Getattr final : public Handler<Getattr> {
  int operator()(const char *path, struct stat *stbuf,
                 struct fuse_file_info *fi) const {
    spdlog::info("Getattr handler called for path: {}", path);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
      return 0;
    }

    if (strcmp(path, "/.containers") == 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
      return 0;
    }

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_getattr(path, stbuf, fi);
    }

    return handle_virtual_file_getattr(path, stbuf, fi);
  }

private:
  int handle_container_getattr(const char *path, struct stat *stbuf,
                               struct fuse_file_info *fi) const {
    spdlog::info("Container Getattr: {}", path);

    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = 0;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    return 0;
  }

  int handle_virtual_file_getattr(const char *path, struct stat *stbuf,
                                  struct fuse_file_info *fi) const {
    spdlog::info("Virtual file Getattr: {}", path);

    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = 0;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_GETATTR