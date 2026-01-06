#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "mapper.hpp"

namespace owl {

template <typename TLoop = ZeroMQLoop, typename TDispatcher = MQDispatcher>
class MQObserver {
  class Handler final : public MQMapper<Handler, TLoop, TDispatcher> {
  public:
    using Base = MQMapper<Handler, TLoop, TDispatcher>;
    using Base::Base;

    void operator()(const std::string &verb_str, const std::string &path_str,
                    const nlohmann::json &msg) {
      try {
        auto [verb, path] = mqmap(verb_str);
        Request req{verb, path, msg};

        this->dispatcher_.dispatch(req);

      } catch (const std::exception &e) {
        std::string id = msg.value("request_id", "");
        this->loop_->sendResponse(id, false, {{"error", e.what()}});
        spdlog::error("MQ error: {}", e.what());
      }
    }
  };

public:
  explicit MQObserver(State &state)
      : handler_(state, std::make_shared<TLoop>([this](auto v, auto p, auto m) {
                   handler_(v, p, m);
                 })),
        runner_(handler_.getLoop()) {}

  void start() { runner_.start("mq_listener"); }
  void stop() { runner_.stop(); }

private:
  Handler handler_;
  SimpleSeparateThreadLoopRunner<TLoop> runner_;
};

} // namespace owl

#endif // OWL_MQ_OBSERVER