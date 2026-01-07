#ifndef OWL_MQ_CONTROLLERS_CONTAINER_STOP
#define OWL_MQ_CONTROLLERS_CONTAINER_STOP

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerStopController final
    : public Controller<ContainerStopController> {
  using Base = Controller<ContainerStopController>;
  using Base::Base;

  template <typename Schema> auto operator()(const nlohmann::json &message) {
    ContainerStopEvent event;

    event.request_id = message["request_id"];
    event.container_id = message["container_id"];

    spdlog::info("ContainerStopController: Stopping container {}",
                 event.container_id);

    return event;
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_STOP