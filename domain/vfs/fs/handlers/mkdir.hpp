#ifndef OWL_VFS_FS_HANDLER_MKDIR
#define OWL_VFS_FS_HANDLER_MKDIR

#include "handler.hpp"
#include <fuse3/fuse.h>

namespace owl {

struct Mkdir final : public Handler<Mkdir> {
  int operator()(const char *path, mode_t mode) const {
    spdlog::info("Mkdir handler called for path: {}", path);

    if (strncmp(path, "/.containers/", 13) == 0) {
      return handle_container_write(path, mode);
    }

    return handle_virtual_file_write(path, mode);
  }

private:
  int handle_container_write(const char *path, mode_t mode) const {
    spdlog::info("Container write: {}", path);
    return 0;
  }

  int handle_virtual_file_write(const char *path, mode_t mode) const {
    spdlog::info("Virtual file write: {}", path);
    return 0;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER_MKDIR