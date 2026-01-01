#include "application.hpp"

namespace owl {

int Application::run(int argc, char *argv[]) {
  return fs_observer_.run(argc, argv);
}

void Application::runHandlers() {

}

} // namespace owl