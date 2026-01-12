#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  mq_observer_.start();

  auto containers = fs_processor_.parseBaseDir();

  if (containers.empty()) {
    spdlog::warn("No containers found in base directory");
  }

  setupFileSystem(containers);

  return fs_observer_.run(argc, argv);
}

void Application::setupFileSystem(const Containers &containers) {
  for (auto &container_data : containers) {
    spdlog::info("Setting up container: {}", container_data.container_id);

    try {
      auto pid_container =
          std::make_shared<ossec::PidContainer>(std::move(container_data));

      auto result = state_.container_manager_.createAndRegisterContainer(
          std::move(pid_container), kModelPath);

      if (!result.is_ok()) {
        spdlog::error("Failed to register container: {}",
                      result.error().what());
      } else {
        spdlog::info("Successfully registered container: {}",
                     container_data.container_id);

        auto container_result =
            state_.container_manager_.getContainer(container_data.container_id);

        if (container_result.is_ok()) {
          auto start_result = container_result.value()->ensureRunning();
          if (!start_result.is_ok()) {
            spdlog::warn("Container {} not started: {}",
                         container_data.container_id,
                         start_result.error().what());
          }

          
        }
      }

    } catch (const std::exception &e) {
      spdlog::error("Error setting up container {}: {}",
                    container_data.container_id, e.what());
    }
  }
}

void Application::stop() {}

} // namespace owl