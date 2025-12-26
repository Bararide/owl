#ifndef OWL_MQ_CONTROLLERS_CONTAINER
#define OWL_MQ_CONTROLLERS_CONTAINER

#include "controller.hpp"
#include "get.hpp"

namespace owl {

struct Container final : public Get<Controller<Container>> {
  nlohmann::json handle(const nlohmann::json &message) {
    try {
      std::string container_id = message["container_id"];
      std::string user_id = message["user_id"];

      spdlog::info("Processing container: {} for user: {}", container_id,
                   user_id);

      return next<File>(container_id, user_id, message);

    } catch (const std::exception &e) {
      spdlog::error("ContainerHandler error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER