#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  mq_observer_.start();

  auto parse_dir_result = fs_processor_.parseBaseDir();

  if (!parse_dir_result.is_ok()) {
    spdlog::critical("Don't correct parse base dir");
    return -1;
  }



  return fs_observer_.run(argc, argv);
}

void Application::stop() {}

} // namespace owl