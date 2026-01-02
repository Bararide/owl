#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "controllers/container.hpp"
#include "controllers/file.hpp"
#include "core/dispatcher.hpp"
#include "core/routing.hpp"
#include "vfs/core/loop/simple_separate_thread.hpp"
#include "vfs/mq/schemas/events.hpp"
#include "vfs/mq/zeromq_loop.hpp"

namespace owl {

using ContainerFilePath = Path<containers_sv, file_sv>;
using GetContainerFilesById =
    Route<Verb::Get, ContainerUserSchema, ContainerFilePath, Container, ById>;

class MQObserver {
public:
  explicit MQObserver(State &state)
      : state_(state), zeromq_loop_(std::make_shared<ZeroMQLoop>(
                           [this](auto verb, auto path, auto msg) {
                             processMQMessage(verb, path, msg);
                           })),
        runner_(zeromq_loop_) {}

  void start() { runner_.start("mq_listener"); }

  void stop() { runner_.stop(); }

  void sendResponse(const std::string &request_id, bool success,
                    const nlohmann::json &data) {
    if (zeromq_loop_) {
      zeromq_loop_->sendResponse(request_id, success, data);
    }
  }

private:
  void processMQMessage(const std::string &verb, const std::string &path,
                        const nlohmann::json &msg) {
    try {
      std::string request_id = msg.value("request_id", "");

      if (verb == "container_create") {
        handleContainerCreate(msg);
      } else if (verb == "get_container_files" ||
                 verb == "get_container_files_and_rebuild") {
        handleGetContainerFiles(msg, verb == "get_container_files_and_rebuild");
      } else if (verb == "container_delete") {
        handleContainerDelete(msg);
      } else if (verb == "file_create" || verb == "create_file") {
        handleFileCreate(msg);
      } else if (verb == "file_delete" || verb == "delete_file") {
        handleFileDelete(msg);
      } else if (verb == "container_stop") {
        handleContainerStop(msg);
      } else if (verb == "semantic_search_in_container") {
        handleSemanticSearchInContainer(msg);
      } else if (verb == "semantic_search") {
        handleSemanticSearch(msg);
      } else {
        sendResponse(request_id, false,
                     {{"error", "Unknown message type: " + verb}});
      }

    } catch (const std::exception &e) {
      std::string request_id = msg.value("request_id", "");
      sendResponse(request_id, false, {{"error", e.what()}});
    }
  }

  void handleContainerCreate(const nlohmann::json &msg) {
    ContainerCreateEvent event;
    event.request_id = msg["request_id"];
    event.container_id = msg["container_id"];
    event.user_id = msg["user_id"];
    event.memory_limit = msg["memory_limit"];
    event.storage_quota = msg["storage_quota"];
    event.file_limit = msg["file_limit"];
    event.privileged = msg["privileged"];
    event.env_label = msg["env_label"];
    event.type_label = msg["type_label"];
    event.commands = msg["commands"].get<std::vector<std::string>>();

    state_.events_.Notify(std::move(event));
  }

  void handleGetContainerFiles(const nlohmann::json &msg, bool rebuild) {
    GetContainerFilesEvent event;
    event.request_id = msg["request_id"];
    event.container_id = msg["container_id"];
    event.user_id = msg["user_id"];
    event.rebuild_index = rebuild;

    state_.events_.Notify(std::move(event));
  }

  void handleContainerDelete(const nlohmann::json &msg) {
    ContainerDeleteEvent event;
    event.request_id = msg["request_id"];
    event.container_id = msg["container_id"];

    state_.events_.Notify(std::move(event));
  }

  void handleFileCreate(const nlohmann::json &msg) {
    FileCreateEvent event;
    event.request_id = msg["request_id"];
    event.path = msg["path"];
    event.content = msg["content"];
    event.user_id = msg["user_id"];
    event.container_id = msg.value("container_id", "");

    state_.events_.Notify(std::move(event));
  }

  void handleFileDelete(const nlohmann::json &msg) {
    FileDeleteEvent event;
    event.request_id = msg["request_id"];
    event.path = msg["path"];
    event.user_id = msg["user_id"];
    event.container_id = msg["container_id"];

    state_.events_.Notify(std::move(event));
  }

  void handleContainerStop(const nlohmann::json &msg) {
    ContainerStopEvent event;
    event.request_id = msg["request_id"];
    event.container_id = msg["container_id"];

    state_.events_.Notify(std::move(event));
  }

  void handleSemanticSearchInContainer(const nlohmann::json &msg) {
    SemanticSearchEvent event;
    event.request_id = msg["request_id"];
    event.query = msg["query"];
    event.limit = msg.value("limit", 10);
    event.user_id = msg["user_id"];
    event.container_id = msg["container_id"];

    state_.events_.Notify(std::move(event));
  }

  void handleSemanticSearch(const nlohmann::json &msg) {
    SemanticSearchEvent event;
    event.request_id = msg["request_id"];
    event.query = msg["query"];
    event.limit = msg.value("limit", 10);

    state_.events_.Notify(std::move(event));
  }

private:
  State &state_;
  std::shared_ptr<ZeroMQLoop> zeromq_loop_;
  SimpleSeparateThreadLoopRunner<ZeroMQLoop> runner_;
};

} // namespace owl

#endif // OWL_MQ_OBSERVER