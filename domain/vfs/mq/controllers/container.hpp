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

  void handle(const nlohmann::json &message) {
    try {
      auto result = ById::validate<ContainerUserSchema>(message);
      if (!result.is_ok()) {
        spdlog::error("Container validation failed: {}", result.error());
        return;
      }

      const auto &[container_id, user_id] = result.value();

      next<File>(container_id, user_id, message);

    } catch (const std::exception &e) {
      spdlog::error("ContainerController error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER