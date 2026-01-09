#ifndef OWL_VFS_CORE_CONTAINER_HANDLER_HPP
#define OWL_VFS_CORE_CONTAINER_HANDLER_HPP

#include "vfs/core/handlers.hpp"
#include "vfs/core/container/ossec_container.hpp"
#include "vfs/mq/operators/resolvers/container/active.hpp"
#include "vfs/mq/operators/resolvers/container/exists.hpp"
#include "vfs/mq/operators/resolvers/container/ownership.hpp"
#include "vfs/mq/operators/resolvers/file/exists.hpp"
#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename Derived, typename EventSchema, typename ValueType,
          typename... Resolvers>
class ContainerHandlerImpl : public EventHandlerBase<Derived, EventSchema> {
protected:
  using Base = EventHandlerBase<Derived, EventSchema>;
  using Base::Base;

  template <typename Handler>
  auto process(const EventSchema &event, Handler &&handler) {
    auto chain =
        ResolverChain<State, EventSchema,
                      ValueType,
                      std::runtime_error, Resolvers...>{Resolvers{}...};

    auto result = withResolvers(
        std::move(chain), std::forward<Handler>(handler))(this->state_, event);

    handleResult(result);

    return result;
  }

private:
  friend Derived;

  template <typename T, typename R>
  static constexpr bool has_on_success = requires(T t, R r) {
    { t.onSuccess(r) } -> std::same_as<void>;
  };

  template <typename T, typename E>
  static constexpr bool has_on_error = requires(T t, E e) {
    { t.onError(e) } -> std::same_as<void>;
  };

  template <typename Value>
  auto callOnSuccess(Value &&value, std::true_type /* has_method */) {
    static_cast<Derived *>(this)->onSuccess(std::forward<Value>(value));
  }

  template <typename Value>
  auto callOnSuccess(Value &&, std::false_type /* has_method */) {}

  template <typename Error>
  auto callOnError(Error &&error, std::true_type /* has_method */) {
    static_cast<Derived *>(this)->onError(std::forward<Error>(error));
  }

  template <typename Error>
  auto callOnError(Error &&error, std::false_type /* has_method */) {
    spdlog::error("Error: {}", error.what());
  }

  template <typename ResultType> void handleResult(ResultType &result) {
    result.match(
        [this](auto &&value) {
          using ValueTypeL = decltype(value);
          callOnSuccess(
              std::forward<ValueTypeL>(value),
              std::bool_constant<has_on_success<Derived, ValueTypeL>>{});
        },
        [this](auto &&error) {
          using ErrorType = decltype(error);
          callOnError(std::forward<ErrorType>(error),
                      std::bool_constant<has_on_error<Derived, ErrorType>>{});
        });
  }
};

template <typename Derived, typename EventSchema>
using ExistingContainerHandler = ContainerHandlerImpl<Derived, EventSchema, OssecContainerPtr, ContainerExists<State, EventSchema>, ContainerOwnership<State, EventSchema>>;

template <typename Derived, typename EventSchema>
using FullContainerHandler = ContainerHandlerImpl<Derived, EventSchema, OssecContainerPtr, ContainerExists<State, EventSchema>, ContainerOwnership<State, EventSchema>, ContainerIsActive<State, EventSchema>>;

template <typename Derived, typename EventSchema>
using CreateContainerHandler = ContainerHandlerImpl<Derived, EventSchema, bool, ContainerNotExists<State, EventSchema>>;

template <typename Derived, typename EventSchema>
using CreateFileHandler = ContainerHandlerImpl<Derived, EventSchema, OssecContainerPtr, ContainerExists<State, EventSchema>, ContainerOwnership<State, EventSchema>, ContainerIsActive<State, EventSchema>, FileNotExists<State, EventSchema>>;

template <typename Derived, typename EventSchema>
using DeleteFileHandler = ContainerHandlerImpl<Derived, EventSchema, OssecContainerPtr, ContainerExists<State, EventSchema>, ContainerOwnership<State, EventSchema>, ContainerIsActive<State, EventSchema>, FileExists<State, EventSchema>>;

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_HANDLER_HPP