#ifndef READ_HANDLER_HPP
#define READ_HANDLER_HPP

#include "handler.hpp"
#include <algorithm>
#include <cstring>

namespace owl::vectorfs {

class ReadHandler final : public BaseHandler<ReadHandler> {
public:
  using BaseHandler<ReadHandler>::BaseHandler;

  int handle(const char *path, char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    const auto *file = state_.get_file(path);
    if (!file) {
      return -ENOENT;
    }

    const std::string &content = file->content;

    if (offset >= static_cast<off_t>(content.size())) {
      return 0;
    }

    size_t bytes_to_read = std::min(size, content.size() - offset);
    memcpy(buf, content.data() + offset, bytes_to_read);

    return bytes_to_read;
  }
};

} // namespace owl::vectorfs

#endif // READ_HANDLER_HPP