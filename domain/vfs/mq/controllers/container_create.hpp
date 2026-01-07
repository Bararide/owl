#ifndef OWL_MQ_CONTROLLERS_CONTAINER_CREATE
#define OWL_MQ_CONTROLLERS_CONTAINER_CREATE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerCreateController final
    : public Controller<ContainerCreateController> {
  using Base = Controller<ContainerCreateController>;
  using Base::Base;

  template <typename Schema> auto operator()(const nlohmann::json &message) {
    ContainerCreateEvent event;

    event.request_id = message["request_id"];
    event.container_id = message["container_id"];
    event.user_id = message["user_id"];
    event.memory_limit = message["memory_limit"];
    event.storage_quota = message["storage_quota"];
    event.file_limit = message["file_limit"];
    event.privileged = message["privileged"];
    event.env_label = message["env_label"];
    event.type_label = message["type_label"];
    event.commands = message["commands"].get<std::vector<std::string>>();

    spdlog::info("ContainerCreateController: Creating container {}",
                 event.container_id);

    return event;
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_CREATE