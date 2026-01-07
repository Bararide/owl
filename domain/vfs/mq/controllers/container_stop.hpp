#ifndef OWL_MQ_CONTROLLERS_CONTAINER_STOP
#define OWL_MQ_CONTROLLERS_CONTAINER_STOP

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerStopController final
    : public Controller<ContainerStopController> {
  using Base = Controller<ContainerStopController>;
  using Base::Base;

  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_STOP