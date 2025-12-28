#ifndef OWL_MQ_CONTROLLERS_FILE
#define OWL_MQ_CONTROLLERS_FILE

#include "../controller.hpp"
#include "../filters/by.hpp"
#include "../schemas/schemas.hpp"

namespace owl {

struct File final : public Controller<File> {
  using Base = Controller<File>;
  using Base::Base;

  nlohmann::json handle(const std::string &container_id,
                        const std::string &user_id,
                        const nlohmann::json &message) {
    try {
      spdlog::info("FileController: container {}, user {}", container_id,
                   user_id);

      // auto r = ById::validate<FileSchema>(message);
      // ...

      return nlohmann::json{{"status", "ok"},
                            {"container_id", container_id},
                            {"user_id", user_id}};

    } catch (const std::exception &e) {
      spdlog::error("FileController error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE