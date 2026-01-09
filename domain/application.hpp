#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#define FUSE_USE_VERSION 31

#include "vfs/core/fsm/states.hpp"
#include "vfs/core/handlers.hpp"
#include "vfs/fs/observer.hpp"
#include "vfs/mq/observer.hpp"
#include "vfs/mq/operators/event_handlers.hpp"

namespace owl {

class Application {
public:
  Application()
      : state_{}, fs_observer_{state_}, mq_observer_{state_},
        event_handlers_{state_} {}

  int run(int argc, char *argv[]);

  void setupFileSystem(const std::vector<ContainerMetadata>& containers);
  void stop();

private:
  State state_;
  FileSystemObserver fs_observer_;
  MQObserver<> mq_observer_;
  Operators event_handlers_;

  FSProcessor fs_processor_{kBaseContainerPath};
};

} // namespace owl

#endif // OWL_APPLICATION