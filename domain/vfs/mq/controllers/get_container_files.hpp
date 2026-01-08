#ifndef OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES
#define OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerGetFilesController final
    : public Controller<ContainerGetFilesController> {
  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES