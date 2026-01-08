#ifndef OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP
#define OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP

#include "vfs/core/handlers.hpp"
#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
class GetContainerFiles
    : public EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema> {
public:
  using Base = EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &event) {
    using Value = std::shared_ptr<IKnowledgeContainer>;
    using Err = std::runtime_error;

    auto chain = createFullContainerResolverChain<State, EventSchema>();

    auto handler = [this](State &state, const EventSchema &event,
                          Value container) -> core::Result<int, Err> {
      spdlog::critical("Обработчик работает, container: {}",
                       container->getId());

      auto files = container->listFiles("/");
      spdlog::info("Found {} files", files.size());

      return core::Result<int, Err>::Ok(static_cast<int>(files.size()));
    };

    auto wrapped = withResolvers<State, EventSchema, Value, Err>(
        std::move(chain), std::move(handler));

    auto result = wrapped(this->state_, event);

    result.match(
        [](int file_count) {
          spdlog::info("Successfully processed {} files", file_count);
        },
        [](const Err &error) {
          spdlog::error("Error in GetContainerFiles: {}", error.what());
        });
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES_HPP