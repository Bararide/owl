#ifndef OWL_API_PUBLISHER
#define OWL_API_PUBLISHER

#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <zmq.hpp>

namespace owl::api::pub {

class MessagePublisher {
public:
  MessagePublisher(const std::string &address = "tcp://127.0.0.1:5555")
      : context_(1), address_(address) {
    connect();
  }

  ~MessagePublisher() {
    if (connected_) {
      socket_.close();
    }
  }

  bool sendContainerCreate(const std::string &container_id,
                           const std::string &user_id, size_t memory_limit,
                           size_t storage_quota, size_t file_limit,
                           bool privileged, const std::string &env_label,
                           const std::string &type_label,
                           const std::vector<std::string> &commands) {

    nlohmann::json message;
    message["type"] = "container_create";
    message["container_id"] = container_id;
    message["user_id"] = user_id;
    message["memory_limit"] = memory_limit;
    message["storage_quota"] = storage_quota;
    message["file_limit"] = file_limit;
    message["privileged"] = privileged;
    message["env_label"] = env_label;
    message["type_label"] = type_label;
    message["commands"] = commands;

    return sendMessage(message.dump());
  }

  bool sendContainerDelete(const std::string &container_id,
                           const std::string &user_id) {
    nlohmann::json message;
    message["user_id"] = user_id;
    message["container_id"] = container_id;

    return sendMessage(message);
  }

  bool sendFileCreate(const std::string &path, const std::string &content,
                      const std::string &user_id,
                      const std::string &container_id) {

    nlohmann::json message;
    message["type"] = "file_create";
    message["path"] = path;
    message["content"] = content;
    message["user_id"] = user_id;
    message["container_id"] = container_id;

    return sendMessage(message.dump());
  }

  bool isConnected() const { return connected_; }

private:
  void connect() {
    try {
      socket_ = zmq::socket_t(context_, ZMQ_PUB);
      socket_.connect(address_);

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      connected_ = true;

      spdlog::info("Connected to ZeroMQ server at {}", address_);
    } catch (const zmq::error_t &e) {
      spdlog::error("Failed to connect to ZeroMQ server: {}", e.what());
      connected_ = false;
    }
  }

  bool sendMessage(const std::string &message) {
    if (!connected_) {
      spdlog::warn("Not connected to ZeroMQ server");
      return false;
    }

    try {
      zmq::message_t zmq_msg(message.size());
      memcpy(zmq_msg.data(), message.data(), message.size());

      auto result = socket_.send(zmq_msg, zmq::send_flags::dontwait);
      if (result) {
        spdlog::debug("Sent message to ZeroMQ server: {} bytes",
                      message.size());
        return true;
      } else {
        spdlog::warn("Failed to send message to ZeroMQ server");
        return false;
      }
    } catch (const zmq::error_t &e) {
      spdlog::error("Error sending message: {}", e.what());
      connected_ = false;
      return false;
    }
  }

  zmq::context_t context_;
  zmq::socket_t socket_;
  std::string address_;
  bool connected_ = false;
};

} // namespace owl::api::pub

#endif // OWL_API_PUBLISHER