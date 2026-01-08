#ifndef OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP
#define OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP

#include "vfs/core/handlers.hpp"
#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct GetContainerFiles
    : EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema> {
  using Base = EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &e) {
    withResolvers(createFullContainerResolverChain<State, EventSchema>(),
                  [this](auto &, auto &, auto c) {
                    auto files = c->listFiles("/");
                    return core::Result<int>::Ok(files.size());
                  })(this->state_, e)
        .handle(
            [](int c) { spdlog::info("Successfully processed {} files", c); },
            [](auto &e) {
              spdlog::error("Error in GetContainerFiles: {}", e.what());
            });
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP