#ifndef OWL_VFS_FS_OBSERVER
#define OWL_VFS_FS_OBSERVER

#define FUSE_USE_VERSION 31

#include "handlers/create.hpp"
#include "handlers/getattr.hpp"
#include "handlers/getxattr.hpp"
#include "handlers/listxattr.hpp"
#include "handlers/mkdir.hpp"
#include "handlers/open.hpp"
#include "handlers/read.hpp"
#include "handlers/readdir.hpp"
#include "handlers/rmdir.hpp"
#include "handlers/setxattr.hpp"
#include "handlers/unlink.hpp"
#include "handlers/utimens.hpp"
#include "handlers/write.hpp"
#include <fuse3/fuse.h>

namespace owl {

class FileSystemObserver {
public:
  static int initialize_fuse(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    auto state = std::make_shared<State>(nullptr, nullptr, nullptr);

    return fuse_main(args.argc, args.argv, &(get_operations()), state.get());
  }

  static inline int getattr_callback(const char *path, struct stat *stbuf,
                                     struct fuse_file_info *fi) {
    return Handler<Getattr>::callback(path, stbuf, fi);
  }

  static inline int readdir_callback(const char *path, void *buf,
                                     fuse_fill_dir_t filler, off_t offset,
                                     struct fuse_file_info *fi,
                                     enum fuse_readdir_flags flags) {
    return Handler<Readdir>::callback(path, buf, filler, offset, fi, flags);
  }

  static inline int open_callback(const char *path, struct fuse_file_info *fi) {
    return Handler<Open>::callback(path, fi);
  }

  static inline int read_callback(const char *path, char *buf, size_t size,
                                  off_t offset, struct fuse_file_info *fi) {
    return Handler<Read>::callback(path, buf, size, offset, fi);
  }

  static inline int write_callback(const char *path, const char *buf,
                                   size_t size, off_t offset,
                                   struct fuse_file_info *fi) {
    return Handler<Write>::callback(path, buf, size, offset, fi);
  }

  static inline int mkdir_callback(const char *path, mode_t mode) {
    return Handler<Mkdir>::callback(path, mode);
  }

  static inline int create_callback(const char *path, mode_t mode,
                                    struct fuse_file_info *fi) {
    return Handler<Create>::callback(path, mode, fi);
  }

  static inline int utimens_callback(const char *path,
                                     const struct timespec tv[2],
                                     struct fuse_file_info *fi) {
    return Handler<Utimens>::callback(path, tv, fi);
  }

  static inline int rmdir_callback(const char *path) {
    return Handler<Rmdir>::callback(path);
  }

  static inline int unlink_callback(const char *path) {
    return Handler<Unlink>::callback(path);
  }

  static inline int getxattr_callback(const char *path, const char *name,
                                      char *value, size_t size) {
    return Handler<Getxattr>::callback(path, name, value, size);
  }

  static inline int setxattr_callback(const char *path, const char *name,
                                      const char *value, size_t size,
                                      int flags) {
    return Handler<Setxattr>::callback(path, name, value, size, flags);
  }

  static inline int listxattr_callback(const char *path, char *list,
                                       size_t size) {
    return Handler<Listxattr>::callback(path, list, size);
  }

  static struct fuse_operations &get_operations() {
    static struct fuse_operations ops = {
        .getattr = getattr_callback,
        .mkdir = mkdir_callback,
        .unlink = unlink_callback,
        .rmdir = rmdir_callback,
        .open = open_callback,
        .read = read_callback,
        .write = write_callback,
        .setxattr = setxattr_callback,
        .getxattr = getxattr_callback,
        .listxattr = listxattr_callback,
        .readdir = readdir_callback,
        .create = create_callback,
        .utimens = utimens_callback,
    };
    return ops;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_OBSERVER