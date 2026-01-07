#ifndef OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES
#define OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerGetFilesController final
    : public Controller<ContainerGetFilesController> {
  using Base = Controller<ContainerGetFilesController>;
  using Base::Base;

  template <typename Schema, typename Event> auto operator()(const nlohmann::json &message) {
    Event event;

    event.request_id = message["request_id"];
    event.container_id = message["container_id"];
    event.user_id = message["user_id"];
    event.rebuild_index = message.value("rebuild_index", false);

    spdlog::info("ContainerGetFilesController: Getting files for container {}",
                 event.container_id);

    return event;
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_GET_FILES