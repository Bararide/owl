#ifndef OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_OWNERSHIP
#define OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_OWNERSHIP

#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename State, typename Event> struct ContainerOwnership {
  auto operator()(State &state, const OssecContainerPtr &container,
                  const Event &event) const -> Result<void> {
    if (container->getOwner() != event.user_id) {
      return Result<void>::Error(
          std::runtime_error("Access denied for user: " + event.user_id));
    }

    return Result<void>::Ok();
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_OWNERSHIP