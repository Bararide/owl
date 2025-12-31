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

  Application() : fs_observer_{state_}, mq_observer_{state_} { runHandlers(); }

  int run(int argc, char *argv[]);

  void runHandlers();

private:
  State state_;
  FileSystemObserver fs_observer_;
  MQObserver mq_observer_;

  
};

} // namespace owl

#endif // OWL_APPLICATION