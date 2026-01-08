#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  mq_observer_.start();
  return fs_observer_.run(argc, argv);
}

void Application::stop() {}

} // namespace owl