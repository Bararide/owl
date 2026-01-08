#ifndef OWL_VFS_CORE_OPERATORS_FILE_DELETE
#define OWL_VFS_CORE_OPERATORS_FILE_DELETE

#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct FileDelete final : DeleteFileHandler<FileCreate<EventSchema>, EventSchema> {
  using Base = DeleteFileHandler<FileCreate<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const auto &e) {
    this->process(e, [this](auto &, auto &, auto c) {
      return core::Result<bool>::Ok(true);
    });
  }

private:
  void onSuccess(bool result) { spdlog::info("Delete success: {}", result); }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_FILE_DELETE