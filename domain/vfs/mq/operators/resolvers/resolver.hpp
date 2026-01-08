#ifndef OWL_MQ_OPERATORS_RESOLVERS_RESOLVER
#define OWL_MQ_OPERATORS_RESOLVERS_RESOLVER

#include <concepts>
#include <functional>
#include <infrastructure/result.hpp>
#include <tuple>
#include <type_traits>

namespace owl {

// ============================================================================
// Концепты для type-safe резолверов
// ============================================================================

template <typename R, typename State, typename Event>
concept BasicResolver = requires(R r, State &s, const Event &e) {
  {
    r(s, e)
  } -> std::same_as<
        core::Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error>>;
};

template <typename R, typename State, typename Event>
concept ResolverValidator =
    requires(R r, State &s, const std::shared_ptr<IKnowledgeContainer> &c,
             const Event &e) {
      { r(s, c, e) } -> std::same_as<core::Result<void, std::runtime_error>>;
    };

template <typename R, typename State, typename Event>
concept Transformer = requires(R r, State &s,
                               const std::shared_ptr<IKnowledgeContainer> &c,
                               const Event &e) {
  {
    r(s, c, e)
  } -> std::same_as<
        core::Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error>>;
};

// ============================================================================
// Базовые резолверы
// ============================================================================

template <typename State, typename Event>
auto containerExistsResolver(State &state, const Event &event)
    -> core::Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error> {

  const auto [container_id, user_id] = event;
  auto container =
      state.container_manager_.getIKnowledgeContainer(container_id);

  if (!container) {
    return core::
        Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error>::Error(
            std::runtime_error("IKnowledgeContainer not found: " +
                               container_id));
  }

  return core::Result<std::shared_ptr<IKnowledgeContainer>,
                      std::runtime_error>::Ok(container);
}

template <typename State, typename Event>
auto containerOwnershipResolver(State &state, const Event &event)
    -> core::Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error> {

  const auto [container_id, user_id] = event;

  return containerExistsResolver(state, event)
      .and_then([&](auto container)
                    -> core::Result<std::shared_ptr<IKnowledgeContainer>,
                                    std::runtime_error> {
        if (container->getOwner() != user_id) {
          return core::Result<std::shared_ptr<IKnowledgeContainer>,
                              std::runtime_error>::
              Error(std::runtime_error("Access denied for user: " + user_id));
        }
        return core::Result<std::shared_ptr<IKnowledgeContainer>,
                            std::runtime_error>::Ok(container);
      });
}

template <typename State, typename Event>
auto containerStatusResolver(State &state, const Event &event)
    -> core::Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error> {

  return containerOwnershipResolver(state, event)
      .and_then([](auto container)
                    -> core::Result<std::shared_ptr<IKnowledgeContainer>,
                                    std::runtime_error> {
        if (!container->isActive()) {
          return core::Result<std::shared_ptr<IKnowledgeContainer>,
                              std::runtime_error>::
              Error(std::runtime_error("IKnowledgeContainer is not active"));
        }
        return core::Result<std::shared_ptr<IKnowledgeContainer>,
                            std::runtime_error>::Ok(container);
      });
}

// ============================================================================
// Валидаторы (работают с уже полученным контейнером)
// ============================================================================

// template <typename State, typename Event>
// auto validateContainerPermissions(const std::string &permission) {
//   return [permission](
//              State &state,
//              const std::shared_ptr<IKnowledgeContainer> &container,
//              const Event &event) -> core::Result<void, std::runtime_error> {
//     const auto [container_id, user_id] = event;

//     if (!state.permission_manager_.check(container->getId(), user_id,
//                                          permission)) {
//       return core::Result<void, std::runtime_error>::Error(std::runtime_error(
//           "Permission denied: " + permission + " for user: " + user_id));
//     }

//     return core::Result<void, std::runtime_error>::Ok();
//   };
// }

// template <typename State, typename Event> auto validateContainerResources() {
//   return [](State &state, const std::shared_ptr<IKnowledgeContainer> &container,
//             const Event &event) -> core::Result<void, std::runtime_error> {
//     if (container->getMemoryUsage() > container->getMemoryLimit() * 0.9) {
//       return core::Result<void, std::runtime_error>::Error(
//           std::runtime_error("IKnowledgeContainer memory limit exceeded"));
//     }

//     return core::Result<void, std::runtime_error>::Ok();
//   };
// }

// ============================================================================
// Композитор резолверов с поддержкой разных типов
// ============================================================================

template <typename State, typename Event, typename... Stages>
class ResolverChain {
  std::tuple<Stages...> stages_;

public:
  explicit ResolverChain(Stages &&...stages)
      : stages_(std::forward<Stages>(stages)...) {}

  auto resolve(State &state, const Event &event)
      -> core::Result<std::shared_ptr<IKnowledgeContainer>,
                      std::runtime_error> {

    return executeChain<0>(state, event, nullptr);
  }

  auto operator()(State &state, const Event &event) {
    return resolve(state, event);
  }

  template <typename NewStage> auto then(NewStage &&new_stage) {
    return std::apply(
        [&](auto &&...stages) {
          return ResolverChain<State, Event, Stages..., NewStage>(
              std::forward<decltype(stages)>(stages)...,
              std::forward<NewStage>(new_stage));
        },
        stages_);
  }

private:
  template <size_t Index,
            typename CurrentResult = std::shared_ptr<IKnowledgeContainer>>
  auto executeChain(State &state, const Event &event, CurrentResult current)
      -> core::Result<std::shared_ptr<IKnowledgeContainer>,
                      std::runtime_error> {

    if constexpr (Index >= sizeof...(Stages)) {
      return core::Result<std::shared_ptr<IKnowledgeContainer>,
                          std::runtime_error>::Ok(current);
    } else {
      auto &stage = std::get<Index>(stages_);

      using StageType = std::decay_t<decltype(stage)>;

      if constexpr (BasicResolver<StageType, State, Event>) {
        auto result = stage(state, event);

        return result.and_then([&](auto container) {
          return executeChain<Index + 1>(state, event, container);
        });

      } else if constexpr (ResolverValidator<StageType, State, Event>) {
        auto result = stage(state, current, event);

        return result.and_then(
            [&]() { return executeChain<Index + 1>(state, event, current); });

      } else if constexpr (Transformer<StageType, State, Event>) {
        auto result = stage(state, current, event);

        return result.and_then([&](auto new_container) {
          return executeChain<Index + 1>(state, event, new_container);
        });

      } else {
        static_assert(!sizeof(StageType),
                      "Unsupported stage type in ResolverChain");
      }
    }
  }
};

// ============================================================================
// Фабричные функции для создания цепочек
// ============================================================================

template <typename State, typename Event> auto createContainerResolverChain() {
  return ResolverChain<State, Event>(containerExistsResolver<State, Event>,
                                     containerOwnershipResolver<State, Event>);
}

template <typename State, typename Event>
auto createFullContainerResolverChain() {
  return ResolverChain<State, Event>(containerExistsResolver<State, Event>,
                                     containerOwnershipResolver<State, Event>,
                                     containerStatusResolver<State, Event>);
}

template <typename State, typename Event>
auto createContainerResolverChainWithPermissions(
    const std::string &permission) {
  return createContainerResolverChain<State, Event>().then(
      validateContainerPermissions<State, Event>(permission));
}

// ============================================================================
// Адаптеры для обработчиков
// ============================================================================

template <typename State, typename Event, typename Handler>
auto withResolvers(ResolverChain<State, Event> &&chain, Handler &&handler) {
  return [chain = std::move(chain), handler = std::forward<Handler>(handler)](
             State &state, const Event &event) mutable {
    return chain(state, event).and_then([&](auto container) {
      return handler(state, event, container);
    });
  };
}

template <typename State, typename Event, typename Handler,
          typename ErrorHandler>
auto withResolvers(ResolverChain<State, Event> &&chain, Handler &&handler,
                   ErrorHandler &&error_handler) {

  return [chain = std::move(chain), handler = std::forward<Handler>(handler),
          error_handler = std::forward<ErrorHandler>(error_handler)](
             State &state, const Event &event) mutable {
    return chain(state, event)
        .and_then(
            [&](auto container) { return handler(state, event, container); })
        .handle([&](const auto &value) { return value; },
                [&](const auto &error) {
                  error_handler(state, event, error);
                  return core::Result<
                      typename std::invoke_result_t<
                          Handler, State &, Event,
                          std::shared_ptr<IKnowledgeContainer>>::value_type,
                      std::runtime_error>::Error(error);
                });
  };
}

template <typename State, typename Event, typename Handler>
auto composeHandler(Handler &&handler) {
  auto chain = createContainerResolverChain<State, Event>();
  return withResolvers(std::move(chain), std::forward<Handler>(handler));
}

template <typename State, typename Event, typename... Resolvers,
          typename Handler>
auto composeHandlerWithChain(ResolverChain<State, Event, Resolvers...> &&chain,
                             Handler &&handler) {

  return withResolvers(std::move(chain), std::forward<Handler>(handler));
}

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_RESOLVER