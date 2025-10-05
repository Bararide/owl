#ifndef OWL_IPC_HPP
#define OWL_IPC_HPP

#include "schemas/fileinfo.hpp"
#include <atomic>
#include <functional>
#include <iceoryx2/v0.7.0/iox2/node.hpp>
#include <iceoryx2/v0.7.0/iox2/publisher.hpp>
#include <iceoryx2/v0.7.0/iox2/service_builder.hpp>
#include <iceoryx2/v0.7.0/iox2/subscriber.hpp>
#include <infrastructure/result.hpp>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace owl {

class IpcService
    : public core::pipeline::PipelineHandler<IpcService, schemas::FileInfo> {
public:
  using MessageCallback = std::function<void(const schemas::FileInfo &)>;

  IpcService() = default;

  explicit IpcService(const std::string &node_name)
      : node_name_(node_name), service_name_("owl_ipc"),
        instance_name_("default") {}

  IpcService(const IpcService &) = delete;
  IpcService(IpcService &&other) noexcept
      : node_name_(std::move(other.node_name_)),
        service_name_(std::move(other.service_name_)),
        instance_name_(std::move(other.instance_name_)),
        node_(std::move(other.node_)), publisher_(std::move(other.publisher_)),
        subscriber_(std::move(other.subscriber_)),
        is_running_(other.is_running_.load()),
        message_callback_(std::move(other.message_callback_)),
        stats_(other.stats_) {
    if (is_running_ && other.receiver_thread_.joinable()) {
      other.is_running_.store(false);
      other.receiver_thread_.join();
      receiver_thread_ = std::thread(&IpcService::receiveMessages, this);
    }
  }

  IpcService &operator=(const IpcService &) = delete;
  IpcService &operator=(IpcService &&) = delete;

  ~IpcService() { stop(); }

  core::Result<bool> initialize() {
    return core::Result<bool>::Ok(true).and_then(
        [this]() -> core::Result<bool> {
          try {
            iox2::NodeBuilder node_builder;

            auto node_name_result = iox2::NodeName::create(node_name_.c_str());
            if (!node_name_result.has_value()) {
              return core::Result<bool>::Error("Invalid node name");
            }

            auto node_result =
                std::move(node_builder).create<iox2::ServiceType::Ipc>();
            if (!node_result.has_value()) {
              return core::Result<bool>::Error(
                  "Failed to create Iceoryx2 node");
            }

            node_ = std::move(node_result.value());

            spdlog::info("IPC Service initialized with node: {}", node_name_);
            return core::Result<bool>::Ok(true);
          } catch (const std::exception &e) {
            return core::Result<bool>::Error(e.what());
          }
        });
  }

  core::Result<bool> startServer() {
    return core::Result<bool>::Ok(true).and_then(
        [this]() -> core::Result<bool> {
          if (is_running_.load()) {
            return core::Result<bool>::Ok(true);
          }

          try {
            auto service_name_result =
                iox2::ServiceName::create(service_name_.c_str());
            if (!service_name_result.has_value()) {
              return core::Result<bool>::Error("Invalid service name");
            }

            auto service_builder = std::move(node_)->service_builder(
                std::move(service_name_result.value()));

            auto service_result =
                std::move(service_builder)
                    .publish_subscribe<iox::ImmutableSlice<const uint8_t>>()
                    .open_or_create();

            if (!service_result.has_value()) {
              return core::Result<bool>::Error("Failed to create service");
            }

            auto service = std::move(service_result.value());
            auto publisher_result = service.publisher_builder().create();
            if (!publisher_result.has_value()) {
              return core::Result<bool>::Error("Failed to create publisher");
            }
            publisher_ = std::move(publisher_result.value());

            is_running_.store(true);
            spdlog::info("IPC Server started on service: {}/{}", service_name_,
                         instance_name_);

            return core::Result<bool>::Ok(true);
          } catch (const std::exception &e) {
            return core::Result<bool>::Error(e.what());
          }
        });
  }

  core::Result<bool> startClient(MessageCallback callback = nullptr) {
    return core::Result<bool>::Ok(true).and_then(
        [this, callback]() -> core::Result<bool> {
          if (is_running_.load()) {
            return core::Result<bool>::Ok(true);
          }

          try {
            auto service_name_result =
                iox2::ServiceName::create(service_name_.c_str());
            if (!service_name_result.has_value()) {
              return core::Result<bool>::Error("Invalid service name");
            }
            auto service_builder = std::move(node_)->service_builder(
                std::move(service_name_result.value()));

            auto service_result =
                std::move(service_builder)
                    .publish_subscribe<iox::ImmutableSlice<const uint8_t>>()
                    .open_or_create();

            if (!service_result.has_value()) {
              return core::Result<bool>::Error("Failed to create service");
            }

            auto service = std::move(service_result.value());
            auto subscriber_result = service.subscriber_builder().create();
            if (!subscriber_result.has_value()) {
              return core::Result<bool>::Error("Failed to create subscriber");
            }
            subscriber_ = std::move(subscriber_result.value());

            message_callback_ = callback;

            receiver_thread_ = std::thread(&IpcService::receiveMessages, this);

            is_running_.store(true);
            spdlog::info("IPC Client started on service: {}/{}", service_name_,
                         instance_name_);

            return core::Result<bool>::Ok(true);
          } catch (const std::exception &e) {
            return core::Result<bool>::Error(e.what());
          }
        });
  }

  void stop() {
    is_running_.store(false);

    if (receiver_thread_.joinable()) {
      receiver_thread_.join();
    }

    publisher_.reset();
    subscriber_.reset();

    spdlog::info("IPC Service stopped");
  }

  core::Result<bool> sendMessage(const schemas::FileInfo &file_info) {
    if (!publisher_.has_value()) {
      return core::Result<bool>::Error("Publisher not initialized");
    }

    return core::Result<bool>::Ok(true).and_then(
        [this, &file_info]() -> core::Result<bool> {
          try {
            auto serialized_data =
                schemas::FileInfoSerializer::serialize(file_info);

            iox::ImmutableSlice<const uint8_t> data_slice(
                serialized_data.data(), serialized_data.size());

            auto send_result = publisher_->send_slice_copy(data_slice);

            if (send_result.has_error()) {
              return core::Result<bool>::Error("Failed to send data");
            }

            stats_.messages_sent++;
            return core::Result<bool>::Ok(true);
          } catch (const std::exception &e) {
            stats_.errors++;
            return core::Result<bool>::Error(e.what());
          }
        });
  }

  core::Result<schemas::FileInfo> handle(schemas::FileInfo &file) override {
    return sendMessage(file)
        .and_then([&file]() -> core::Result<schemas::FileInfo> {
          return core::Result<schemas::FileInfo>::Ok(file);
        })
        .match(
            [](schemas::FileInfo result) -> core::Result<schemas::FileInfo> {
              return core::Result<schemas::FileInfo>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<schemas::FileInfo> {
              spdlog::error("IPC handling failed: {}", error.what());
              return core::Result<schemas::FileInfo>::Error(
                  "IPC handling failed");
            });
  }

  void await() override {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  void setMessageCallback(MessageCallback callback) {
    message_callback_ = callback;
  }

  struct IpcStats {
    size_t messages_sent = 0;
    size_t messages_received = 0;
    size_t errors = 0;
  };

  IpcStats getStats() const { return stats_; }

  bool isRunning() const { return is_running_.load(); }
  bool isServer() const { return publisher_.has_value(); }
  bool isClient() const { return subscriber_.has_value(); }

private:
  void receiveMessages() {
    spdlog::info("IPC message receiver thread started");

    while (is_running_.load()) {
      if (!subscriber_.has_value()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }

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

        if (file_info_result.is_ok()) {
          stats_.messages_received++;

          if (message_callback_) {
            message_callback_(file_info_result.value());
          }

          spdlog::debug("IPC message received for file: {}",
                        file_info_result.value().name.value_or("unknown"));
        } else {
          stats_.errors++;
          spdlog::warn("Failed to deserialize IPC message: {}",
                       file_info_result.error().what());
        }

      } catch (const std::exception &e) {
        stats_.errors++;
        spdlog::error("Error in IPC receiver thread: {}", e.what());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    spdlog::info("IPC message receiver thread stopped");
  }

private:
  std::string node_name_;
  std::string service_name_;
  std::string instance_name_;

  std::optional<iox2::Node<iox2::ServiceType::Ipc>> node_;
  std::optional<iox2::Publisher<iox2::ServiceType::Ipc,
                                iox::ImmutableSlice<const uint8_t>, void>>
      publisher_;
  std::optional<iox2::Subscriber<iox2::ServiceType::Ipc,
                                 iox::ImmutableSlice<const uint8_t>, void>>
      subscriber_;

  std::atomic<bool> is_running_{false};
  std::thread receiver_thread_;
  MessageCallback message_callback_;
  IpcStats stats_;
};
} // namespace owl

#endif // OWL_IPC_HPP