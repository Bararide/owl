#ifndef OWL_API_PUBLISHER
#define OWL_API_PUBLISHER

#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <zmq.hpp>

#include "requests.hpp"

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
        spdlog::debug("Published message to ZeroMQ server: {} bytes",
                      message.size());
        return true;
      } else {
        spdlog::warn("Failed to publish message to ZeroMQ server");
        return false;
      }
    } catch (const zmq::error_t &e) {
      spdlog::error("Error sending message: {}", e.what());
      connected_ = false;
      return false;
    }
  }

  bool sendContainerMetrics(const std::string &user_id,
                            const std::string &container_id,
                            validate::GetContainerMetrics &metrics) {
    nlohmann::json message;
    message["type"] = "get_container_metrics";
    message["user_id"] = user_id;
    message["container_id"] = container_id;
    message["request_id"] = generate_request_id();

    if (!sendMessage(message.dump())) {
      return false;
    }

    std::string response_data;
    if (!receiveResponse(response_data)) {
      return false;
    }

    try {
      auto response = nlohmann::json::parse(response_data);

      if (response.contains("success") && response["success"].get<bool>()) {
        metrics.memory_limit = response["memory_limit"].get<uint16_t>();
        metrics.cpu_limit = response["cpu_limit"].get<uint16_t>();
        return true;
      } else {
        spdlog::error("Response indicates failure");
        return false;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to parse response: {}", e.what());
      return false;
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
    message["type"] = "container_delete";
    message["user_id"] = user_id;
    message["container_id"] = container_id;

    return sendMessage(message.dump());
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

  bool sendFileDelete(const std::string &path, const std::string &user_id,
                      const std::string &container_id) {
    nlohmann::json message;
    message["type"] = "file_delete";
    message["path"] = path;
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
      spdlog::info("Connected to ZeroMQ publisher at {}", address_);
      
    } catch (const zmq::error_t &e) {
      spdlog::error("Failed to connect to ZeroMQ publisher: {}", e.what());
      connected_ = false;
    }
  }

  bool receiveResponse(std::string &response) {
    try {
      zmq::message_t reply;
      auto result = socket_.recv(reply, zmq::recv_flags::none);

      if (result) {
        response = std::string(static_cast<char *>(reply.data()), reply.size());
        spdlog::debug("Received response: {} bytes", response.size());
        return true;
      } else {
        spdlog::warn("Failed to receive response");
        return false;
      }
    } catch (const zmq::error_t &e) {
      spdlog::error("Error receiving response: {}", e.what());
      return false;
    }
  }

  std::string generate_request_id() {
    static std::atomic<uint64_t> counter{0};
    return std::to_string(counter++) + "-" +
           std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count());
  }

  zmq::context_t context_;
  zmq::socket_t socket_;
  std::string address_;
  bool connected_ = false;
};

} // namespace owl::api::pub

#endif // OWL_API_PUBLISHER