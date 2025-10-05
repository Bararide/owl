#ifndef OWL_IPC_BASE_HPP
#define OWL_IPC_BASE_HPP

#include "schemas/fileinfo.hpp"
#include <atomic>
#include <functional>
#include <iceoryx2/v0.7.0/iox2/node.hpp>
#include <iceoryx2/v0.7.0/iox2/publisher.hpp>
#include <iceoryx2/v0.7.0/iox2/service_builder.hpp>
#include <iceoryx2/v0.7.0/iox2/subscriber.hpp>
#include <infrastructure/result.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>

namespace owl {

class IpcBaseService {
public:
  using MessageCallback = std::function<void(const schemas::FileInfo &)>;
  using PublisherType =
      iox2::Publisher<iox2::ServiceType::Ipc,
                      iox::ImmutableSlice<const uint8_t>, void>;
  using SubscriberType =
      iox2::Subscriber<iox2::ServiceType::Ipc,
                       iox::ImmutableSlice<const uint8_t>, void>;

  IpcBaseService() = default;

  explicit IpcBaseService(const std::string &node_name,
                          const std::string &service_name = "owl_ipc",
                          const std::string &instance_name = "default")
      : node_name_(node_name), service_name_(service_name),
        instance_name_(instance_name) {}

  virtual ~IpcBaseService() { stop(); }

  core::Result<bool> initialize() {
    try {
      iox2::NodeBuilder node_builder;
      auto node_name_result = iox2::NodeName::create(node_name_.c_str());
      if (!node_name_result.has_value()) {
        return core::Result<bool>::Error("Invalid node name");
      }

      auto node_result =
          std::move(node_builder).create<iox2::ServiceType::Ipc>();
      if (!node_result.has_value()) {
        return core::Result<bool>::Error("Failed to create Iceoryx2 node");
      }

      node_ = std::move(node_result.value());
      spdlog::info("IPC Base Service initialized with node: {}", node_name_);
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<std::shared_ptr<PublisherType>> createPublisher() {
    try {
      auto service_name_result =
          iox2::ServiceName::create(service_name_.c_str());
      if (!service_name_result.has_value()) {
        return core::Result<std::shared_ptr<PublisherType>>::Error(
            "Invalid service name");
      }

      auto service_builder = std::move(node_)->service_builder(
          std::move(service_name_result.value()));

      auto service_result =
          std::move(service_builder)
              .publish_subscribe<iox::ImmutableSlice<const uint8_t>>()
              .open_or_create();

      if (!service_result.has_value()) {
        return core::Result<std::shared_ptr<PublisherType>>::Error(
            "Failed to create service");
      }

      auto service = std::move(service_result.value());
      auto publisher_result = service.publisher_builder().create();
      if (!publisher_result.has_value()) {
        return core::Result<std::shared_ptr<PublisherType>>::Error(
            "Failed to create publisher");
      }

      auto publisher =
          std::make_shared<PublisherType>(std::move(publisher_result.value()));
      spdlog::info("Publisher created for service: {}/{}", service_name_,
                   instance_name_);
      return core::Result<std::shared_ptr<PublisherType>>::Ok(
          std::move(publisher));
    } catch (const std::exception &e) {
      return core::Result<std::shared_ptr<PublisherType>>::Error(e.what());
    }
  }

  core::Result<std::shared_ptr<SubscriberType>> createSubscriber() {
    try {
      auto service_name_result =
          iox2::ServiceName::create(service_name_.c_str());
      if (!service_name_result.has_value()) {
        return core::Result<std::shared_ptr<SubscriberType>>::Error(
            "Invalid service name");
      }

      auto service_builder = std::move(node_)->service_builder(
          std::move(service_name_result.value()));

      auto service_result =
          std::move(service_builder)
              .publish_subscribe<iox::ImmutableSlice<const uint8_t>>()
              .open_or_create();

      if (!service_result.has_value()) {
        return core::Result<std::shared_ptr<SubscriberType>>::Error(
            "Failed to create service");
      }

      auto service = std::move(service_result.value());
      auto subscriber_result = service.subscriber_builder().create();
      if (!subscriber_result.has_value()) {
        return core::Result<std::shared_ptr<SubscriberType>>::Error(
            "Failed to create subscriber");
      }

      auto subscriber = std::make_shared<SubscriberType>(
          std::move(subscriber_result.value()));
      spdlog::info("Subscriber created for service: {}/{}", service_name_,
                   instance_name_);
      return core::Result<std::shared_ptr<SubscriberType>>::Ok(
          std::move(subscriber));
    } catch (const std::exception &e) {
      return core::Result<std::shared_ptr<SubscriberType>>::Error(e.what());
    }
  }

  void startReceiving(std::shared_ptr<SubscriberType> subscriber,
                      MessageCallback callback) {
    if (!subscriber) {
      spdlog::error("Cannot start receiving: subscriber is null");
      return;
    }

    message_callback_ = callback;
    subscriber_ = subscriber;
    is_running_.store(true);

    receiver_thread_ = std::thread([this]() { this->receiveMessages(); });
  }

  virtual void stop() {
    is_running_.store(false);
    if (receiver_thread_.joinable()) {
      receiver_thread_.join();
    }
    spdlog::info("IPC Base Service stopped");
  }

  bool isRunning() const { return is_running_.load(); }

protected:
  void receiveMessages() {
    spdlog::info("IPC message receiver thread started");

    while (is_running_.load() && subscriber_) {
      try {
        auto receive_result = subscriber_->receive();
        if (!receive_result.has_value() ||
            !receive_result.value().has_value()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          continue;
        }

        auto sample = std::move(receive_result.value().value());
        auto payload = sample.payload();

        std::vector<uint8_t> message_data(
            payload.data(), payload.data() + payload.number_of_elements());

        auto file_info_result =
            schemas::FileInfoSerializer::deserialize(message_data);

        if (file_info_result.is_ok() && message_callback_) {
          message_callback_(file_info_result.value());
        }

      } catch (const std::exception &e) {
        spdlog::error("Error in IPC receiver thread: {}", e.what());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    spdlog::info("IPC message receiver thread stopped");
  }

  std::string node_name_;
  std::string service_name_;
  std::string instance_name_;

  std::optional<iox2::Node<iox2::ServiceType::Ipc>> node_;
  std::shared_ptr<SubscriberType> subscriber_;

  std::atomic<bool> is_running_{false};
  std::thread receiver_thread_;
  MessageCallback message_callback_;
};

} // namespace owl

#endif // OWL_IPC_BASE_HPP