#ifndef OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER
#define OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER

#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename State, typename Event> struct ContainerExists final {
  auto operator()(State &state, const Event &event) const
      -> Result<std::shared_ptr<IKnowledgeContainer>> {
    auto container = state.container_manager_.getContainer(event.container_id);

    if (!container) {
      return Result<std::shared_ptr<IKnowledgeContainer>>::Error(
          std::runtime_error("IKnowledgeContainer not found: " +
                             event.container_id));
    }

    return Result<std::shared_ptr<IKnowledgeContainer>>::Ok(container);
  }
};

template <typename State, typename Event> struct ContainerNotExists {
  auto operator()(State &state, const Event &event) const -> Result<bool> {
    auto container = state.container_manager_.getContainer(event.container_id);

    if (container) {
      return Result<bool>::Error(std::runtime_error(
          "Container already exists: " + event.container_id));
    }

    return Result<bool>::Ok(true);
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER