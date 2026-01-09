#ifndef OWL_MQ_CONTROLLERS_FILE_CREATE
#define OWL_MQ_CONTROLLERS_FILE_CREATE

#include "vfs/mq/controller.hpp"

namespace owl {

struct FileCreateController final : public Controller<FileCreateController> {
  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE_CREATE