#ifndef OWL_MQ_CONTROLLERS_CONTAINER
#define OWL_MQ_CONTROLLERS_CONTAINER

#include "../controller.hpp"
#include "../filters/by.hpp"
#include "../schemas/schemas.hpp"
#include "file.hpp"

namespace owl {

struct Container final : public Controller<Container> {
  using Base = Controller<Container>;
  using Base::Base;

  nlohmann::json handle(const nlohmann::json &message) {
    try {
      auto result = ById::validate<ContainerSchema>(message);
      if (!result.is_ok()) {
        spdlog::error("Container validation failed: {}", result.error());
        throw std::runtime_error("Validation failed");
      }

      const auto &schema = result.value();
      std::string container_id = schema.container_id;
      std::string user_id = message.at("user_id").get<std::string>();

      spdlog::info("Processing container: {} for user: {}", container_id,
                   user_id);

      return next<File>(container_id, user_id, message);

    } catch (const std::exception &e) {
      spdlog::error("ContainerController error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER