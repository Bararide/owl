#ifndef OWL_API_SUBSCRIBER_HPP
#define OWL_API_SUBSCRIBER_HPP

#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <zmq.hpp>

namespace owl::api::sub {

class MessageSubscriber {
public:
  using MessageHandler = std::function<void(const nlohmann::json &)>;

  MessageSubscriber(const std::string &address = "tcp://localhost:5556")
      : context_(1), address_(address), running_(false) {}

  ~MessageSubscriber() { stop(); }

  void registerHandler(MessageHandler handler) { message_handler_ = handler; }

  void start() {
    if (running_) {
      spdlog::warn("Subscriber already running");
      return;
    }

    try {
      socket_ = zmq::socket_t(context_, ZMQ_SUB);
      socket_.connect(address_);
      socket_.set(zmq::sockopt::subscribe,
                  "");

      running_ = true;
      subscriber_thread_ = std::thread(&MessageSubscriber::run, this);

      spdlog::info("ZeroMQ subscriber started on {}", address_);
    } catch (const zmq::error_t &e) {
      spdlog::error("Failed to start ZeroMQ subscriber: {}", e.what());
      throw;
    }
  }

  void stop() {
    running_ = false;
    if (subscriber_thread_.joinable()) {
      subscriber_thread_.join();
    }
    if (socket_.handle() != nullptr) {
      socket_.close();
    }
    spdlog::info("ZeroMQ subscriber stopped");
  }

  bool isRunning() const { return running_; }

private:
  void run() {
    while (running_) {
      try {
        zmq::message_t message;
        auto result = socket_.recv(message, zmq::recv_flags::dontwait);

        if (result && message.size() > 0) {
          processMessage(
              std::string(static_cast<char *>(message.data()), message.size()));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      } catch (const zmq::error_t &e) {
        if (e.num() != EAGAIN) {
          spdlog::error("ZeroMQ error in subscriber: {}", e.what());
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      } catch (const std::exception &e) {
        spdlog::error("Exception in ZeroMQ subscriber: {}", e.what());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  void processMessage(const std::string &message) {
    try {
      auto json_msg = nlohmann::json::parse(message);

      if (message_handler_) {
        message_handler_(json_msg);
      }
    } catch (const nlohmann::json::exception &e) {
      spdlog::error("Failed to parse JSON message: {}", e.what());
    } catch (const std::exception &e) {
      spdlog::error("Error processing message: {}", e.what());
    }
  }

  zmq::context_t context_;
  zmq::socket_t socket_;
  std::string address_;
  MessageHandler message_handler_;
  std::thread subscriber_thread_;
  std::atomic<bool> running_;
};

} // namespace owl::api::sub

#endif // OWL_API_SUBSCRIBER_HPP