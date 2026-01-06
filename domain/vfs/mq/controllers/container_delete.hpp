#ifndef OWL_MQ_CONTROLLERS_CONTAINER_DELETE
#define OWL_MQ_CONTROLLERS_CONTAINER_DELETE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct ContainerDeleteController final
    : public Controller<ContainerDeleteController> {
  using Base = Controller<ContainerDeleteController>;
  using Base::Base;

  template <typename Schema> void handle(const nlohmann::json &message) {
    ContainerDeleteEvent event;

    event.request_id = message["request_id"];
    event.container_id = message["container_id"];

    spdlog::info("ContainerDeleteController: Deleting container {}",
                 event.container_id);

    state_.events_.Notify(std::move(event));
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER_DELETE