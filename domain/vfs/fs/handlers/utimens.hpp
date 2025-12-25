#ifndef OWL_VFS_FS_HANDLER_UTIMENS
#define OWL_VFS_FS_HANDLER_UTIMENS

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Utimens final : public Handler<Utimens> {
  int operator()(const char *path, const struct timespec tv[2],
                 struct fuse_file_info *fi) const {
    spdlog::info("Utimens handler called for path: {}", path);

    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_UTIMENS