#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  mq_observer_.start();

  auto parse_dir_result = fs_processor_.parseBaseDir();

  if (parse_dir_result.size() == 0) {
    spdlog::critical("Don't correct parse base dir");
    return -1;
  }

  setupFileSystem(parse_dir_result);

  return fs_observer_.run(argc, argv);
}

void Application::setupFileSystem(
    const std::vector<ContainerMetadata> &containers) {
  for (auto &metadata : containers) {
    spdlog::info("Registering container: {}", metadata.container_id);

    auto container_result =
        State::OssecContainerT::createFromMetadata(metadata, kModelPath);

    if (!container_result.is_ok()) {
      spdlog::error("Failed to create container {}: {}", metadata.container_id,
                    container_result.error().what());
      continue;
    }

    auto register_result =
        state_.container_manager_.registerContainer(container_result.value());

    if (!register_result.is_ok()) {
      spdlog::error("Failed to register container {}: {}",
                    metadata.container_id, register_result.error().what());
    } else {
      spdlog::info("Successfully registered container: {}",
                   metadata.container_id);
    }
  }
}

void Application::stop() {}

} // namespace owl