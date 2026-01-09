#ifndef OWL_MQ_CONTROLLER
#define OWL_MQ_CONTROLLER

#include "validator.hpp"
#include "vfs/domain.hpp"
#include "vfs/core/schemas/schemas.hpp"

namespace owl {

template <typename Derived>
class Controller : public Validator<Controller<Derived>> {
protected:
  State &state_;

public:
  using Base = Validator<Controller<Derived>>;
  using Base::Base;

  explicit Controller(State &state) : state_(state) {}

  template <typename Schema, typename NextHandler, typename... Args>
  auto next(Args &&...args) -> decltype(auto) {
    NextHandler next_handler(state_);
    return next_handler.template handle<Schema>(std::forward<Args>(args)...);
  }

  template <typename Schema, typename Event, typename... Args>
  void handle(Args &&...args) {
    spdlog::critical("Controller::operator() вызван");

    auto result =
        static_cast<Derived *>(this)->template operator()<Schema, Event>(
            std::forward<Args>(args)...);

    result.handle(
        [this](const Event &event) {
          spdlog::critical("Controller отправляет Notify");
          spdlog::critical("Тип события: {}", typeid(Event).name());
          state_.events_.template Notify<Event>(std::move(event));
        },
        [](auto &err) {
          spdlog::error("Controller error: {}", err);
          throw err;
        });
  }

private:
  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_CONTROLLER