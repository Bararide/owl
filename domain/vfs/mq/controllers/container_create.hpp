#ifndef OWL_MQ_CONTROLLERS_CONTAINER_CREATE
#define OWL_MQ_CONTROLLERS_CONTAINER_CREATE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerCreateController final : public Controller<ContainerCreateController> {
  template <typename Schema, typename Event>
  auto operator()(const auto &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_CREATE