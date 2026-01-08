#ifndef OWL_VFS_CORE_OPERATORS_FILE_CREATE
#define OWL_VFS_CORE_OPERATORS_FILE_CREATE

#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct FileCreate final : CreateFileHandler<FileCreate<EventSchema>, EventSchema> {
  using Base = CreateFileHandler<FileCreate<EventSchema>, EventSchema>;
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

#endif // OWL_VFS_CORE_OPERATORS_FILE_CREATE