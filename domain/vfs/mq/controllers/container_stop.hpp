#ifndef OWL_MQ_CONTROLLERS_CONTAINER_STOP
#define OWL_MQ_CONTROLLERS_CONTAINER_STOP

#include "vfs/mq/controller.hpp"

namespace owl {

struct ContainerStopController final
    : public Controller<ContainerStopController> {
  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_STOP