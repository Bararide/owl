#ifndef OWL_VFS_FS_HANDLER_RMDIR
#define OWL_VFS_FS_HANDLER_RMDIR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Rmdir final : public Handler<Rmdir> {
  int operator()(const char *path) const {
    spdlog::info("Rmdir handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_RMDIR