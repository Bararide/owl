#ifndef OWL_MQ_CONTROLLERS_FILE
#define OWL_MQ_CONTROLLERS_FILE

#include "../controller.hpp"
#include "../filters/by.hpp"
#include "../operators/get.hpp"

namespace owl {

struct File final : public Controller<File> {
  using Base = Controller<File>;
  using Base::Base;

  nlohmann::json handle(const std::string &container_id,
                        const std::string &user_id,
                        const nlohmann::json &message) {
    try {

      return {};

    } catch (const std::exception &e) {
      spdlog::error("File error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE