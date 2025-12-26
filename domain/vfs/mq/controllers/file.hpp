#ifndef OWL_MQ_CONTROLLERS_FILE
#define OWL_MQ_CONTROLLERS_FILE

#include "controller.hpp"
#include <nlohmann/json.hpp>

namespace owl {

struct FileHandler final : public Controller<FileHandler> {
  nlohmann::json handle(const std::string &container_id,
                        const std::string &user_id,
                        const nlohmann::json &message) {
    try {

      return {};

    } catch (const std::exception &e) {
      spdlog::error("FileHandler error: {}", e.what());
      throw;
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_FILE