#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#define FUSE_USE_VERSION 31

#include "vfs/core/fsm/states.hpp"
#include "vfs/core/handlers.hpp"
#include "vfs/core/loop/loop.hpp"
#include "vfs/fs/observer.hpp"
#include "vfs/mq/observer.hpp"
#include "vfs/mq/operators/get/container_files.hpp"
#include "vfs/mq/schemas/events.hpp"

namespace owl {

class Application {
public:
  Application()
      : state_{}, fs_observer_{state_}, mq_observer_{state_},
        event_handlers_{state_}, event_loop_{} {

    setupEventSubscriptions();
  }

  int run(int argc, char *argv[]);

  void stop();

private:
  void setupEventSubscriptions() {
    state_.events_.Subscribe<ContainerCreateEvent>([this](const auto &event) {
      mq_observer_.sendResponse(event.request_id, true,
                                {{"status", "created"}});
    });

    state_.events_.Subscribe<GetContainerFilesEvent>([this](const auto &event) {
      nlohmann::json files{};
      mq_observer_.sendResponse(event.request_id, true, {{"files", files}});
    });
  }

private:
  State state_;
  FileSystemObserver fs_observer_;
  MQObserver mq_observer_;
  EventHandlers<GetContainerFiles<GetContainerFilesEvent>> event_handlers_;
  EventLoop event_loop_;
};

} // namespace owl

#endif // OWL_APPLICATION