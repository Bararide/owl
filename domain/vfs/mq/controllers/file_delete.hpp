#ifndef OWL_MQ_CONTROLLERS_FILE_DELETE
#define OWL_MQ_CONTROLLERS_FILE_DELETE

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct FileDeleteController final : public Controller<FileDeleteController> {
  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE_DELETE