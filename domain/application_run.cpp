#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  event_loop_.start();
  mq_observer_.start();
  return 0;
  // return fs_observer_.run(argc, argv);
}

void Application::stop() { event_loop_.stop(); }

} // namespace owl