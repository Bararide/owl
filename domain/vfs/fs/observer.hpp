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
  explicit FileSystemObserver(State &state) : state_(state) {}

  int run(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    return fuse_main(args.argc, args.argv, &ops_, this);
  }

private:
  State &state_;

  static FileSystemObserver *getSelf() {
    return static_cast<FileSystemObserver *>(fuse_get_context()->private_data);
  }

  static int getattr(const char *path, struct stat *stbuf,
                     struct fuse_file_info *fi) {
    return Handler<Getattr>::callback(path, stbuf, fi);
  }

  //     return Handler<Getattr>::callback(get_self()->state_, path, stbuf, fi);

  static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags) {
    return Handler<Readdir>::callback(path, buf, filler, offset, fi, flags);
  }

  static int open(const char *path, struct fuse_file_info *fi) {
    return Handler<Open>::callback(path, fi);
  }

  static int read(const char *path, char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi) {
    return Handler<Read>::callback(path, buf, size, offset, fi);
  }

  static int write(const char *path, const char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    return Handler<Write>::callback(path, buf, size, offset, fi);
  }

  static int mkdir(const char *path, mode_t mode) {
    return Handler<Mkdir>::callback(path, mode);
  }

  static int create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    return Handler<Create>::callback(path, mode, fi);
  }

  static int utimens(const char *path, const struct timespec tv[2],
                     struct fuse_file_info *fi) {
    return Handler<Utimens>::callback(path, tv, fi);
  }

  static int rmdir(const char *path) { return Handler<Rmdir>::callback(path); }

  static int unlink(const char *path) {
    return Handler<Unlink>::callback(path);
  }

  static int getxattr(const char *path, const char *name, char *value,
                      size_t size) {
    return Handler<Getxattr>::callback(path, name, value, size);
  }

  static int setxattr(const char *path, const char *name, const char *value,
                      size_t size, int flags) {
    return Handler<Setxattr>::callback(path, name, value, size, flags);
  }

  static int listxattr(const char *path, char *list, size_t size) {
    return Handler<Listxattr>::callback(path, list, size);
  }

  static inline struct fuse_operations ops_ = {
      .getattr = getattr,
      .mkdir = mkdir,
      .unlink = unlink,
      .rmdir = rmdir,
      .open = open,
      .read = read,
      .write = write,
      .setxattr = setxattr,
      .getxattr = getxattr,
      .listxattr = listxattr,
      .readdir = readdir,
      .create = create,
      .utimens = utimens,
  };
};

} // namespace owl
#endif