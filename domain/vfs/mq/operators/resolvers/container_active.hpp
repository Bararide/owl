#ifndef OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_ACTIVE
#define OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_ACTIVE

#include "resolver.hpp"

namespace owl {

template <typename State, typename Event> struct ContainerIsActive {
  auto operator()(State &,
                  const std::shared_ptr<IKnowledgeContainer> &container,
                  const Event &) const -> Result<void, std::runtime_error> {
    if (!container->isAvailable()) {
      return Result<void, std::runtime_error>::Error(
          std::runtime_error("IKnowledgeContainer is not active"));
    }
    return Result<void, std::runtime_error>::Ok();
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER_ACTIVE