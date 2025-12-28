#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "vfs/fs/observer.hpp"
#include "vfs/mq/observer.hpp"

namespace owl {

class Application {
public:
  Application(const Application &other) = delete;
  Application(Application &&other) = delete;

  Application &operator=(const Application &other) = delete;
  Application &operator=(Application &&other) = delete;

  Application() : observer_{state_} {}

  int run(int argc, char *argv[]) { return observer_.run(argc, argv); }

private:
  State state_;
  FileSystemObserver observer_;
};

} // namespace owl

#endif // OWL_APPLICATION