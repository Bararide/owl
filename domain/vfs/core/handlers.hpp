#ifndef OWL_VFS_CORE_HANDLERS
#define OWL_VFS_CORE_HANDLERS

#include "vfs/domain.hpp"

namespace owl {

template <typename Derived, typename EventSchema> class EventHandlerBase {
public:
  using StateType = State;
  
  explicit EventHandlerBase(State &state) : state_{state} {
    state_.events_.Subscribe<EventSchema>([this](const EventSchema &event) {
      static_cast<Derived &>(*this)(event);
    });
  }

  ~EventHandlerBase() = default;

  EventHandlerBase(const EventHandlerBase &) = delete;
  EventHandlerBase(EventHandlerBase &&) = delete;
  EventHandlerBase &operator=(const EventHandlerBase &) = delete;
  EventHandlerBase &operator=(EventHandlerBase &&) = delete;

protected:
  State &state_;

  friend Derived;
};

template <typename ConcreteHandler> class EventHandlerWrapper {
public:
  explicit EventHandlerWrapper(State &state)
      : handler_{std::make_unique<ConcreteHandler>(state)} {}

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

private:
  std::unique_ptr<ConcreteHandler> handler_;
};

template <typename... ConcreteHandlers> class EventHandlers final {
public:
  explicit EventHandlers(State &state)
      : handlers_(
            std::make_tuple(EventHandlerWrapper<ConcreteHandlers>(state)...)) {}

  ~EventHandlers() = default;

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

  template <typename... Args> void operator()(Args &&...args) {
    std::apply(
        [&args...](auto &...handlers) {
          (handlers(std::forward<Args>(args)...), ...);
        },
        handlers_);
  }

private:
  std::tuple<EventHandlerWrapper<ConcreteHandlers>...> handlers_;
};

} // namespace owl

#endif // OWL_VFS_CORE_HANDLERS