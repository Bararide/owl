#ifndef OWL_MQ_CONTROLLER
#define OWL_MQ_CONTROLLER

#include "validator.hpp"
#include "vfs/domain.hpp"

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

  template <typename Schema, typename... Args> void handle(Args &&...args) {
    state_.events_.template Notify<
        decltype(static_cast<Derived *>(this)->template operator()<Schema>(
            std::forward<Args>(args)...))>(
        static_cast<Derived *>(this)->template operator()<Schema>(
            std::forward<Args>(args)...));
  }

private:
  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_CONTROLLER