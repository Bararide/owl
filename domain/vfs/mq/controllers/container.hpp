#ifndef OWL_MQ_CONTROLLERS_CONTAINER
#define OWL_MQ_CONTROLLERS_CONTAINER

#include "file.hpp"

namespace owl {

struct Container final : public Controller<Container> {
  using Base = Controller<Container>;
  using Base::Base;

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