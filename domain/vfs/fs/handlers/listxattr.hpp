#ifndef OWL_VFS_FS_HANDLER_LISTXATTR
#define OWL_VFS_FS_HANDLER_LISTXATTR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Listxattr final : public Handler<Listxattr> {
  int operator()(const char *path, char *list, size_t size) const {
    spdlog::info("Listxattr handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_LISTXATTR