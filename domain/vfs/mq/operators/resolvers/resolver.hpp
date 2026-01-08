#ifndef OWL_MQ_OPERATORS_RESOLVERS_RESOLVER_HPP
#define OWL_MQ_OPERATORS_RESOLVERS_RESOLVER_HPP

#include <concepts>
#include <functional>
#include <infrastructure/result.hpp>
#include <memory>
#include <tuple>
#include <type_traits>

namespace owl {

template <typename T, typename Err = std::runtime_error>
using Result = core::Result<T, Err>;

template <typename R, typename State, typename Event, typename Value,
          typename Err>
concept BasicResolver = requires(R r, State &s, const Event &e) {
  { r(s, e) } -> std::same_as<Result<Value, Err>>;
};

template <typename R, typename State, typename Event, typename Value,
          typename Err>
concept ValidatorResolver =
    requires(R r, State &s, const Value &v, const Event &e) {
      { r(s, v, e) } -> std::same_as<Result<void, Err>>;
    };

template <typename R, typename State, typename Event, typename Value,
          typename Err>
concept Transformer = requires(R r, State &s, Value &v, const Event &e) {
  { r(s, v, e) } -> std::same_as<Result<Value, Err>>;
};

template <typename State, typename Event, typename Value, typename Err,
          typename... Stages>
class ResolverChain {
  std::tuple<Stages...> stages_;

public:
  using StateType = State;
  using EventType = Event;
  using ValueType = Value;
  using ErrorType = Err;

  explicit ResolverChain(Stages... stages) : stages_(std::move(stages)...) {}

  ResolverChain(ResolverChain &&) noexcept = default;
  ResolverChain(const ResolverChain &) = default;

  auto resolve(State &state, const Event &event) const -> Result<Value, Err> {
    return executeChain<0>(state, event, Value{});
  }

  auto operator()(State &state, const Event &event) const {
    return resolve(state, event);
  }

  template <typename NewStage> auto then(NewStage &&new_stage) const {
    return std::apply(
        [&](const auto &...stages) {
          using NewStageT = std::decay_t<NewStage>;
          return ResolverChain<State, Event, Value, Err, Stages..., NewStageT>(
              stages..., std::forward<NewStage>(new_stage));
        },
        stages_);
  }

private:
  template <std::size_t Index>
  auto executeChain(State &state, const Event &event,
                    Value current) const -> Result<Value, Err> {
    if constexpr (Index >= sizeof...(Stages)) {
      return Result<Value, Err>::Ok(std::move(current));
    } else {
      const auto &stage = std::get<Index>(stages_);
      using StageType = std::decay_t<decltype(stage)>;

      if constexpr (BasicResolver<StageType, State, Event, Value, Err>) {
        auto result = stage(state, event);
        return result.and_then([&](auto &&val) {
          return executeChain<Index + 1>(state, event,
                                         std::forward<decltype(val)>(val));
        });
      } else if constexpr (ValidatorResolver<StageType, State, Event, Value,
                                             Err>) {
        auto result = stage(state, current, event);
        return result.and_then([&]() {
          return executeChain<Index + 1>(state, event, std::move(current));
        });
      } else if constexpr (Transformer<StageType, State, Event, Value, Err>) {
        auto result = stage(state, current, event);
        return result.and_then([&](auto &&val) {
          return executeChain<Index + 1>(state, event,
                                         std::forward<decltype(val)>(val));
        });
      } else {
        static_assert(!sizeof(StageType),
                      "Unsupported stage type in ResolverChain");
      }
    }
  }
};

template <typename State, typename Event, typename Value, typename Err,
          typename... Stages, typename Handler>
auto withResolvers(ResolverChain<State, Event, Value, Err, Stages...> chain,
                   Handler &&handler) {
  return [chain = std::move(chain), handler = std::forward<Handler>(handler)](
             State &state, const Event &event) mutable {
    return chain(state, event).and_then([&](Value v) {
      return handler(state, event, std::move(v));
    });
  };
}

template <typename State, typename Event, typename Value, typename Err,
          typename... Stages, typename Handler, typename ErrorHandler>
auto withResolvers(ResolverChain<State, Event, Value, Err, Stages...> chain,
                   Handler &&handler, ErrorHandler &&error_handler) {
  return [chain = std::move(chain), handler = std::forward<Handler>(handler),
          error_handler = std::forward<ErrorHandler>(error_handler)](
             State &state, const Event &event) mutable {
    auto result = chain(state, event).and_then([&](Value v) {
      return handler(state, event, std::move(v));
    });

    if (!result.is_ok()) {
      error_handler(state, event, result.error());
    }

    return result;
  };
}

template <typename State, typename Event, typename Handler>
auto composeHandler(Handler &&handler) {
  auto chain = createContainerResolverChain<State, Event>();
  using Chain = decltype(chain);
  using Value = typename Chain::ValueType;
  using Err = typename Chain::ErrorType;
  return withResolvers<State, Event, Value, Err>(
      std::move(chain), std::forward<Handler>(handler));
}

template <typename State, typename Event, typename Value, typename Err,
          typename... Resolvers, typename Handler>
auto composeHandlerWithChain(
    ResolverChain<State, Event, Value, Err, Resolvers...> chain,
    Handler &&handler) {
  return withResolvers<State, Event, Value, Err>(
      std::move(chain), std::forward<Handler>(handler));
}

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_RESOLVER_HPP