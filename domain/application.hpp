#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#define FUSE_USE_VERSION 31

#include "vfs/core/fsm/states.hpp"
#include "vfs/core/handlers.hpp"
#include "vfs/core/operators/get/container_files.hpp"
#include "vfs/fs/observer.hpp"
#include "vfs/mq/observer.hpp"

namespace owl {

class Application {
public:
  Application()
      : state_{}, fs_observer_{state_}, mq_observer_{state_},
        event_handlers_{state_} {}

  int run(int argc, char *argv[]);
  void runHandlers();

private:
  State state_;
  FileSystemObserver fs_observer_;
  MQObserver mq_observer_;

  EventHandlers<GetContainerFiles<GetContainerFilesEvent>> event_handlers_;
};

} // namespace owl
#endif // OWL_APPLICATION