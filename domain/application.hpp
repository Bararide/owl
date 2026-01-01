#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#define FUSE_USE_VERSION 31

#include "vfs/core/fsm/states.hpp"
#include "vfs/core/handlers.hpp"
#include "vfs/core/loop/loop.hpp"
#include "vfs/fs/observer.hpp"
#include "vfs/mq/observer.hpp"
#include "vfs/mq/operators/get/container_files.hpp"

namespace owl {

class Application {
public:
  Application()
      : state_{}, fs_observer_{state_}, mq_observer_{state_},
        event_handlers_{state_}, event_loop_{} {}

  int run(int argc, char *argv[]);
  void runHandlers();

  void stop();

private:
  State state_;
  FileSystemObserver fs_observer_;
  MQObserver mq_observer_;

  EventHandlers<GetContainerFiles<GetContainerFilesEvent>> event_handlers_;

  EventLoop event_loop_;
};

} // namespace owl

#endif // OWL_APPLICATION