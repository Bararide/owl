#ifndef OWL_MQ_CONTROLLER
#define OWL_MQ_CONTROLLER

#include "vfs/domain.hpp"

namespace owl {

template <typename Derived> class Controller {
protected:
  State &state_;

public:
  explicit Controller(State &state) : state_{state} {}

  template <typename NextHandler, typename... Args>
  auto next(Args &&...args) -> decltype(auto) {
    NextHandler nextHandler{};
    return nextHandler.handle(std::forward<Args>(args)...);
  }

private:
  friend Derived;
};

template <typename... Handlers> class HandlerPipeline {
  State &state_;

public:
  HandlerPipeline(State &state) : state_(state) {}

  template <typename Input> auto process(Input &&input) {
    return process_impl<Handlers...>(std::forward<Input>(input));
  }

private:
  template <typename Handler, typename... Rest, typename Input>
  auto process(Input &&input) {
    Handler handler(state_);

    if constexpr (sizeof...(Rest) == 0) {
      return handler.handle(std::forward<Input>(input));
    } else {
      auto result = handler.handle(std::forward<Input>(input));

      return std::apply(
          [&](auto &&...args) {
            return process_impl<Rest...>(std::forward<decltype(args)>(args)...);
          },
          std::move(result));
    }
  }
};

// using Pipeline = HandlerPipeline<ContainerHandler, FileHandler>;

// nlohmann::json result = Pipeline(state).process(message);

} // namespace owl

#endif // OWL_MQ_CONTROLLER