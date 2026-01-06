#ifndef OWL_VFS_MQ_ZEROMQ_LOOP
#define OWL_VFS_MQ_ZEROMQ_LOOP

#include "vfs/core/socket/socket.hpp"

namespace owl {

class ZeroMQLoop {
public:
  using MessageHandler = std::function<void(
      const std::string &, const std::string &, const nlohmann::json &)>;

  explicit ZeroMQLoop(MessageHandler handler)
      : handler_(std::move(handler)),
        subscriber_(SocketType::Sub, "tcp://127.0.0.1:5555"),
        publisher_(SocketType::Pub, "tcp://*:5556"), is_active_(false) {

    spdlog::info("ZeroMQLoop INIT: Subscriber connecting to "
                 "127.0.0.1:5555, Publisher binding to *:5556");

    subscriber_.setReceiveTimeout(1000);
    subscriber_.setLinger(0);
    subscriber_.setSubscribe("");

    publisher_.setSendTimeout(1000);
    publisher_.setLinger(0);
    publisher_.setImmediate(true);

    spdlog::info("ZeroMQLoop initialized successfully");
  }

  void start() {
    if (is_active_) {
      return;
    }
    is_active_ = true;
  }

  void stop() { is_active_ = false; }

  void update() {
    if (!is_active_) {
      return;
    }

    if (auto msg = subscriber_.receiveString(zmq::recv_flags::dontwait)) {
      try {
        auto json_msg = nlohmann::json::parse(*msg);

        std::string verb = json_msg.value("type", "");
        std::string path = json_msg.value("path", "");

        if (handler_) {
          handler_(verb, path, json_msg);
        }

      } catch (const nlohmann::json::exception &e) {
        // Логирование
      }
    }
  }

  void sendResponse(const std::string &request_id, bool success,
                    const nlohmann::json &data) {
    nlohmann::json response = {{"request_id", request_id},
                               {"success", success},
                               {"timestamp", std::time(nullptr)}};

    if (success) {
      response["data"] = data;
    } else {
      response["error"] = data.value("error", "Unknown error");
    }

    publisher_.send(response.dump());
  }

  void setIsActive(bool active) { is_active_ = active; }
  bool getIsActive() const { return is_active_; }

private:
  MessageHandler handler_;
  Socket subscriber_;
  Socket publisher_;
  std::atomic<bool> is_active_;
};

} // namespace owl

#endif // OWL_VFS_MQ_ZEROMQ_LOOP