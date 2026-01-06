#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "vfs/mq/controllers/controllers.hpp"
#include "vfs/core/loop/simple_separate_thread.hpp"
#include "vfs/mq/zeromq_loop.hpp"

namespace owl {

class MQObserver {
public:
  explicit MQObserver(State &state)
      : state_(state), dispatcher_(state),
        zeromq_loop_(std::make_shared<ZeroMQLoop>(
            [this](auto verb_str, auto path_str, auto msg) {
              processMessage(verb_str, path_str, msg);
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
  void processMessage(const std::string &verb_str, const std::string &path_str,
                      const nlohmann::json &msg) {
    try {
      std::string request_id = msg.value("request_id", "");

      auto [verb, path] = mapMQToRoute(verb_str, path_str, msg);

      Request req{verb, path, msg};

      spdlog::info("Processing MQ message: {} {} -> {} {}", verb_str, path_str,
                   verbToString(verb), path);

      dispatcher_.dispatch(req);

    } catch (const std::exception &e) {
      std::string request_id = msg.value("request_id", "");
      sendResponse(request_id, false, {{"error", e.what()}});
      spdlog::error("Error processing MQ message: {}", e.what());
    }
  }

  std::pair<Verb, std::string> mapMQToRoute(const std::string &verb_str,
                                            const std::string &path_str,
                                            const nlohmann::json &msg) {
    if (verb_str == "container_create") {
      return {Verb::Post, "container/create"};
    } else if (verb_str == "get_container_files" ||
               verb_str == "get_container_files_and_rebuild") {
      return {Verb::Get, "container/files"};
    } else if (verb_str == "container_delete") {
      return {Verb::Delete, "container/delete"};
    } else if (verb_str == "file_create" || verb_str == "create_file") {
      return {Verb::Post, "file/create"};
    } else if (verb_str == "file_delete" || verb_str == "delete_file") {
      return {Verb::Delete, "file/delete"};
    } else if (verb_str == "container_stop") {
      return {Verb::Post, "container/stop"};
    } else if (verb_str == "semantic_search_in_container" ||
               verb_str == "semantic_search") {
      return {Verb::Post, "search/semantic"};
    }

    throw std::runtime_error("Unknown MQ command: " + verb_str);
  }

  std::string verbToString(Verb verb) {
    switch (verb) {
    case Verb::Get:
      return "GET";
    case Verb::Post:
      return "POST";
    case Verb::Put:
      return "PUT";
    case Verb::Delete:
      return "DELETE";
    default:
      return "UNKNOWN";
    }
  }

private:
  State &state_;
  MQDispatcher dispatcher_;
  std::shared_ptr<ZeroMQLoop> zeromq_loop_;
  SimpleSeparateThreadLoopRunner<ZeroMQLoop> runner_;
};

} // namespace owl

#endif // OWL_MQ_OBSERVER