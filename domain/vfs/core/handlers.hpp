#ifndef OWL_VFS_CORE_HANDLERS
#define OWL_VFS_CORE_HANDLERS

#include "vfs/core/loop/loop.hpp"
#include "vfs/domain.hpp"
#include <typeindex>
#include <unordered_map>

namespace owl {

template <typename Derived, typename EventSchema> class EventHandlerBase {
public:
  using StateType = State;
  using EventType = EventSchema;

  explicit EventHandlerBase(State &state, EventLoop &loop)
      : state_{state}, loop_{loop} {

    state_.events_.Subscribe<EventSchema>([this](const EventSchema &event) {
      loop_.post([this, event]() { static_cast<Derived &>(*this)(event); });
    });
  }

  ~EventHandlerBase() = default;

  EventHandlerBase(const EventHandlerBase &) = delete;
  EventHandlerBase(EventHandlerBase &&) = delete;
  EventHandlerBase &operator=(const EventHandlerBase &) = delete;
  EventHandlerBase &operator=(EventHandlerBase &&) = delete;

protected:
  State &state_;
  EventLoop &loop_;

  friend Derived;
};

template <typename ConcreteHandler> class EventHandlerWrapper {
public:
  explicit EventHandlerWrapper(State &state, EventLoop &loop)
      : handler_{std::make_unique<ConcreteHandler>(state, loop)} {}

  EventHandlerWrapper(EventHandlerWrapper &&other) noexcept = default;
  EventHandlerWrapper &
  operator=(EventHandlerWrapper &&other) noexcept = default;

  EventHandlerWrapper(const EventHandlerWrapper &) = delete;
  EventHandlerWrapper &operator=(const EventHandlerWrapper &) = delete;

  template <typename... Args> void operator()(Args &&...args) {
    (*handler_)(std::forward<Args>(args)...);
  }

  ConcreteHandler &get() { return *handler_; }
  const ConcreteHandler &get() const { return *handler_; }

  template <typename Event> void dispatchIfMatch(const Event &event) {
    if constexpr (std::is_same_v<Event, typename ConcreteHandler::EventType>) {
      (*handler_)(event);
    }
  }

private:
  std::unique_ptr<ConcreteHandler> handler_;
};

template <typename Handler> struct EventTypeExtractor;

template <template <typename> class Handler, typename Event>
struct EventTypeExtractor<Handler<Event>> {
  using type = Event;
};

template <typename Handler>
using EventTypeOf = typename EventTypeExtractor<Handler>::type;

template <typename... ConcreteHandlers> class EventHandlers final {
public:
  explicit EventHandlers(State &state)
      : state_{state}, loop_{std::make_shared<EventLoop>()},
        handlers_{std::make_tuple(
            EventHandlerWrapper<ConcreteHandlers>(state, *loop_)...)} {

    loop_->start();

    initDispatcher();
  }

  ~EventHandlers() { loop_->stop(); }

  EventHandlers(const EventHandlers &) = delete;
  EventHandlers(EventHandlers &&) = default;
  EventHandlers &operator=(const EventHandlers &) = delete;
  EventHandlers &operator=(EventHandlers &&) = default;

  template <typename HandlerType> HandlerType &get() {
    return std::get<EventHandlerWrapper<HandlerType>>(handlers_).get();
  }

  template <typename HandlerType> const HandlerType &get() const {
    return std::get<EventHandlerWrapper<HandlerType>>(handlers_).get();
  }

  template <typename Event> void dispatch(const Event &event) {
    loop_->post([this, event]() {
      std::apply(
          [&event](auto &...handlers) {
            (handlers.template dispatchIfMatch(event), ...);
          },
          handlers_);
    });
  }

  template <typename... Args> void operator()(Args &&...args) {
    static_assert(sizeof...(Args) == 0,
                  "Use dispatch(event) instead of operator()");
  }

private:
  void initDispatcher() { registerDispatcher<ConcreteHandlers...>(); }

  template <typename FirstHandler, typename... RestHandlers>
  void registerDispatcher() {
    using EventType = EventTypeOf<FirstHandler>;

    state_.events_.template Subscribe<EventType>(
        [this](const EventType &event) { this->dispatch(event); });

    if constexpr (sizeof...(RestHandlers) > 0) {
      registerDispatcher<RestHandlers...>();
    }
  }

private:
  std::shared_ptr<EventLoop> loop_;
  std::tuple<EventHandlerWrapper<ConcreteHandlers>...> handlers_;
  State &state_;
};

} // namespace owl

#endif // OWL_VFS_CORE_HANDLERS