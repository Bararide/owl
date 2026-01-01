#ifndef OWL_VFS_MQ_ZEROMQ_LOOP
#define OWL_VFS_MQ_ZEROMQ_LOOP

#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <zmq.hpp>

namespace owl {

class ZeroMQLoop {
public:
  using MessageHandler =
      std::function<void(const std::string &verb, const std::string &path,
                         const nlohmann::json &msg)>;

  explicit ZeroMQLoop(MessageHandler handler)
      : handler_(std::move(handler)), context_(1), is_active_(false) {}

  ~ZeroMQLoop() { stop(); }

  void start() {
    if (is_active_)
      return;

    try {
      subscriber_ = std::make_unique<zmq::socket_t>(context_, ZMQ_SUB);
      subscriber_->bind("tcp://*:5555");
      subscriber_->setsockopt(ZMQ_SUBSCRIBE, "", 0);

      publisher_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PUB);
      publisher_->bind("tcp://*:5556");

      is_active_ = true;

    } catch (const zmq::error_t &e) {
      // Логирование ошибки
    }
  }

  void stop() {
    if (!is_active_)
      return;

    is_active_ = false;

    if (subscriber_) {
      subscriber_->close();
      subscriber_.reset();
    }

    if (publisher_) {
      publisher_->close();
      publisher_.reset();
    }
  }

  void update() {
    if (!is_active_)
      return;

    try {
      zmq::message_t message;
      auto result = subscriber_->recv(message, zmq::recv_flags::dontwait);

      if (result && message.size() > 0) {
        std::string message_str(static_cast<char *>(message.data()),
                                message.size());

        try {
          auto json_msg = nlohmann::json::parse(message_str);

          std::string verb = json_msg.value("type", "");
          std::string path = json_msg.value("path", "");

          if (handler_) {
            handler_(verb, path, json_msg);
          }

        } catch (const std::exception &e) {
          // Логирование ошибки парсинга
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    } catch (const zmq::error_t &e) {
      // Логирование ошибки ZeroMQ
    }
  }

  void sendResponse(const std::string &request_id, bool success,
                    const nlohmann::json &data) {
    if (!publisher_ || !is_active_)
      return;

    try {
      nlohmann::json response = {{"request_id", request_id},
                                 {"success", success},
                                 {"timestamp", std::time(nullptr)}};

      if (success) {
        response["data"] = data;
      } else {
        response["error"] = data.value("error", "Unknown error");
      }

      std::string response_str = response.dump();
      zmq::message_t msg(response_str.size());
      memcpy(msg.data(), response_str.data(), response_str.size());

      publisher_->send(msg, zmq::send_flags::dontwait);

    } catch (const std::exception &e) {
      // Логирование ошибки
    }
  }

  void setIsActive(bool active) { is_active_ = active; }
  bool getIsActive() const { return is_active_; }

private:
  MessageHandler handler_;
  zmq::context_t context_;
  std::unique_ptr<zmq::socket_t> subscriber_;
  std::unique_ptr<zmq::socket_t> publisher_;
  std::atomic<bool> is_active_;
};

} // namespace owl

#endif // OWL_VFS_MQ_ZEROMQ_LOOP