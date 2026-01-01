#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  event_loop_.start();
  return fs_observer_.run(argc, argv);
}

void Application::stop() { event_loop_.stop(); }

void Application::runHandlers() {}

} // namespace owl