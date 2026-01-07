#ifndef OWL_MQ_CONTROLLERS_FILE_DELETE
#define OWL_MQ_CONTROLLERS_FILE_DELETE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct FileDeleteController final : public Controller<FileDeleteController> {
  using Base = Controller<FileDeleteController>;
  using Base::Base;

  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    Event event;

    event.request_id = message["request_id"];
    event.path = message["path"];
    event.user_id = message["user_id"];
    event.container_id = message["container_id"];

    spdlog::info("FileDeleteController: Deleting file {} from container {}",
                 event.path, event.container_id);

    return event;
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE_DELETE