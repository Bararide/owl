#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "controllers/container.hpp"
#include "controllers/file.hpp"
#include "core/dispatcher.hpp"
#include "core/routing.hpp"
#include "mq_loop.hpp"
#include "vfs/core/loop/simple_separate_thread.hpp"

namespace owl {

using ContainerFilePath = Path<containers_sv, file_sv>;
using GetContainerFilesById =
    Route<Verb::Get, ContainerUserSchema, ContainerFilePath, Container, ById>;

class MQObserver {
public:
  explicit MQObserver(State &state)
      : dispatcher_(state), state_(state),
        mq_loop_(
            std::make_shared<MQLoop>([this](auto verb, auto path, auto msg) {
              onMessage(verb, path, msg);
            })),
        runner_(mq_loop_) {}

  void start() { runner_.start("mq_listener"); }

  void stop() { runner_.stop(); }

  void onMessage(const std::string &verb_str, const std::string &path,
                 const nlohmann::json &msg) {
    Verb v = parseVerb(verb_str);
    Request req{v, path, msg};

    dispatcher_.dispatch(std::move(req));
  }

  void sendResponse(const std::string &queue, const std::string &correlation_id,
                    const nlohmann::json &response) {
    mq_loop_->sendMessage(
        queue, "RESPONSE", "",
        {{"correlation_id", correlation_id}, {"response", response}});
  }

private:
  using MyDispatcher =
      Dispatcher<GetContainerFilesById /*, другие маршруты... */>;

  MyDispatcher dispatcher_;
  State &state_;
  std::shared_ptr<MQLoop> mq_loop_;
  SimpleSeparateThreadLoopRunner<MQLoop> runner_;

  static Verb parseVerb(const std::string &v) {
    if (v == "GET")
      return Verb::Get;
    if (v == "POST")
      return Verb::Post;
    if (v == "PUT")
      return Verb::Put;
    if (v == "DELETE")
      return Verb::Delete;
    throw std::runtime_error("Unknown verb: " + v);
  }
};

} // namespace owl

#endif // OWL_MQ_OBSERVER