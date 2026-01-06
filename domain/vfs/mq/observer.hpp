#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "mapper.hpp"

namespace owl {

class MQObserver {
  class Handler final : public MQMapper<Handler> {
  public:
    using Base = MQMapper<Handler>;
    using Base::Base;

    void operator()(const std::string &verb_str, const std::string &path_str,
                    const nlohmann::json &msg) {
      try {
        auto [verb, path] = mqmap(verb_str);
        Request req{verb, path, msg};

        dispatcher_.dispatch(req);

      } catch (const std::exception &e) {
        std::string id = msg.value("request_id", "");
        loop_->sendResponse(id, false, {{"error", e.what()}});
        spdlog::error("MQ error: {}", e.what());
      }
    }
  };

public:
  explicit MQObserver(State &state)
      : handler_(state,
                 std::make_shared<ZeroMQLoop>([this](auto v, auto p, auto m) {
                   handler_.processMessage(v, p, m);
                 })),
        runner_(handler_.getLoop()) {}

  void start() { runner_.start("mq_listener"); }
  void stop() { runner_.stop(); }

private:
  Handler handler_;
  SimpleSeparateThreadLoopRunner<ZeroMQLoop> runner_;
};

} // namespace owl

#endif // OWL_MQ_OBSERVER