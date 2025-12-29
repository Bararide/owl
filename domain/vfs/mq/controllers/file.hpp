#ifndef OWL_MQ_CONTROLLERS_FILE
#define OWL_MQ_CONTROLLERS_FILE

#include "../controller.hpp"
#include "../filters/by.hpp"
#include "../schemas/events.hpp"
#include "../schemas/schemas.hpp"

namespace owl {

struct File final : public Controller<File> {
  using Base = Controller<File>;
  using Base::Base;

  void handle(const std::string &container_id, const std::string &user_id,
              const nlohmann::json &message) {
    state_.events_.NotifyAsync(
        ContainerUserSchema{.container_id = container_id, .user_id = user_id});
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE