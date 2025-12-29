#ifndef OWL_MQ_CONTROLLER
#define OWL_MQ_CONTROLLER

#include "vfs/domain.hpp"

namespace owl {

template <typename Derived> class Controller {
protected:
  State &state_;

public:
  explicit Controller(State &state) : state_(state) {}

  template <typename Schema, typename NextHandler, typename... Args>
  auto next(Args &&...args) -> decltype(auto) {
    NextHandler next_handler(state_);
    return next_handler.template handle<Schema>(std::forward<Args>(args)...);
  }

  template <typename Schema, typename... Args>
  auto handle(Args &&...args) -> decltype(auto) {
    return static_cast<Derived *>(this)->template handle<Schema>(
        std::forward<Args>(args)...);
  }

private:
  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_CONTROLLER