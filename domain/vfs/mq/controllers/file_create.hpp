#ifndef OWL_MQ_CONTROLLERS_FILE_CREATE
#define OWL_MQ_CONTROLLERS_FILE_CREATE

#include "../controller.hpp"
#include "../schemas/events.hpp"

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