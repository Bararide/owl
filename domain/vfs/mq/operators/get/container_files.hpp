#ifndef OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP
#define OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP

#include "vfs/core/handlers.hpp"
#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename EventSchema>
class GetContainerFiles
    : public EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema> {
public:
  using Base = EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &event) {
    auto handler = composeHandler<State, EventSchema>(
        [this](State &state, const EventSchema &event,
               auto container) -> core::Result<int, std::runtime_error> {
          spdlog::critical("Обработчик работает");

          return core::Result<int, std::runtime_error>::Ok(0);
        });

    handler(this->state_, event)
        .match(
            [this](const auto &files) {
              spdlog::info("Successfully processed {} files", files.size());
            },
            [this](const std::runtime_error &error) {
              spdlog::error("Error in GetContainerFiles: {}", error.what());
            });
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP