#ifndef OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER
#define OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER

#include <concepts>
#include <functional>
#include <infrastructure/result.hpp>

namespace owl::resolvers {

template <typename State> class ContainerResolver {
  State &state_;

public:
  explicit ContainerResolver(State &state) : state_(state) {}

  template <typename Event>
    requires requires(Event e) {
      { e.container_id } -> std::convertible_to<std::string>;
      { e.user_id } -> std::convertible_to<std::string>;
    }
  auto resolve(const Event &event)
      -> core::Result<std::shared_ptr<Container>, std::runtime_error> {
    const auto [container_id, user_id] = event;

    auto container = state_.container_manager_.getContainer(container_id);

    if (!container) {
      return core::Result<std::shared_ptr<Container>, std::runtime_error>::
          Error(std::runtime_error("Container not found: " + container_id));
    }

    if (container->getOwner() != user_id) {
      return core::Result<std::shared_ptr<Container>, std::runtime_error>::
          Error(std::runtime_error("Access denied for user: " + user_id));
    }

    return core::Result<std::shared_ptr<Container>, std::runtime_error>::Ok(
        container);
  }

  template <typename Event> auto operator()(const Event &event) {
    return resolve(event);
  }

  template <typename Handler> auto wrap(Handler &&handler) {
    return [this, handler = std::forward<Handler>(handler)](
               State &state, const auto &event) mutable {
      return resolve(event).and_then(
          [&](auto container) { return handler(state, event, container); });
    };
  }
};

template <typename State, typename Handler>
auto withContainer(State &state, Handler &&handler) {
  ContainerResolver<State> resolver(state);
  return resolver.wrap(std::forward<Handler>(handler));
}

} // namespace owl::resolvers

#endif // OWL_MQ_OPERATORS_RESOLVERS_CHECK_CONTAINER