#ifndef OWL_VFS_FS_HANDLER_UNLINK
#define OWL_VFS_FS_HANDLER_UNLINK

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Unlink final : public Handler<Unlink> {
  int operator()(const char *path) const {
    spdlog::info("Unlink handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_UNLINK