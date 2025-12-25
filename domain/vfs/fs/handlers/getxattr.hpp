#ifndef OWL_VFS_FS_HANDLER_GETXATTR
#define OWL_VFS_FS_HANDLER_GETXATTR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Getxattr final : public Handler<Getxattr> {
  int operator()(const char *path, const char *name, char *value,
                 size_t size) const {
    spdlog::info("Getxattr handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_GETXATTR