#include "application.hpp"

int main(int argc, char* argv[]) {
  owl::app::VectorFSApplication app(argc, argv);
  return app.run();
}