#ifndef OWL_MQ_CONTROLLERS_FILE_CREATE
#define OWL_MQ_CONTROLLERS_FILE_CREATE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct FileCreateController final : public Controller<FileCreateController> {
  using Base = Controller<FileCreateController>;
  using Base::Base;

  template <typename Schema> void handle(const nlohmann::json &message) {
    FileCreateEvent event;

    event.request_id = message["request_id"];
    event.path = message["path"];
    event.content = message["content"];
    event.user_id = message["user_id"];
    event.container_id = message.value("container_id", "");

    spdlog::info("FileCreateController: Creating file {} in container {}",
                 event.path, event.container_id);

    state_.events_.Notify(std::move(event));
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE_CREATE