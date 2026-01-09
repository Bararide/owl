#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  mq_observer_.start();

  auto parse_dir_result = fs_processor_.parseBaseDir();

  if (!parse_dir_result.is_ok()) {
    spdlog::critical("Don't correct parse base dir");
    return -1;
  }

  setupFileSystem(parse_dir_result.value());

  return fs_observer_.run(argc, argv);
}

void Application::setupFileSystem(const std::vector<ContainerMetadata>& containers) {
    for(auto &a : containers) {
        spdlog::critical("ID: {}", a.container_id);
    }
}

void Application::stop() {}

} // namespace owl