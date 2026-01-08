#ifndef OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER
#define OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER

#include "resolver.hpp"

namespace owl {

template <typename State, typename Event> struct ContainerExists {
  auto operator()(State &state, const Event &event) const
      -> Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error> {
    auto container = state.container_manager_.getContainer(event.container_id);

    if (!container) {
      return Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error>::
          Error(std::runtime_error("IKnowledgeContainer not found: " +
                                   event.container_id));
    }

    return Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error>::Ok(
        container);
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER