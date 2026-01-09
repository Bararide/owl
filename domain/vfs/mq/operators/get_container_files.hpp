#ifndef OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP
#define OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP

#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct GetContainerFiles final
    : ExistingContainerHandler<GetContainerFiles<EventSchema>, EventSchema> {
  using Base =
      ExistingContainerHandler<GetContainerFiles<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &event) {
    this->process(event, [](auto &, auto &, auto c) {
      return core::Result<int>::Ok(c->listFiles("/").unwrap().size());
    });
  }

private:
  void onSuccess(int count) { spdlog::info("Files: {}", count); }
};

} // namespace owl

#endif