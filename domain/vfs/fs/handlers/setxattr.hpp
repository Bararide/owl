#ifndef OWL_VFS_FS_HANDLER_SETXATTR
#define OWL_VFS_FS_HANDLER_SETXATTR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Setxattr final : public Handler<Setxattr> {
  int operator()(const char *path, const char *name, const char *value,
                 size_t size, int flags) const {
    spdlog::info("Setxattr handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_SETXATTR