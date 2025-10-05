#ifndef OWL_IPC_PIPELINE_HANDLER_HPP
#define OWL_IPC_PIPELINE_HANDLER_HPP

#include "ipc_handler_base.hpp"

namespace owl {

class IpcPipelineHandler : public IpcHandlerBase<IpcPipelineHandler> {
public:
  using PublisherType =
      iox2::Publisher<iox2::ServiceType::Ipc,
                      iox::ImmutableSlice<const uint8_t>, void>;

  IpcPipelineHandler() = default;

  IpcPipelineHandler(const IpcPipelineHandler &other)
      : publisher_(other.publisher_) {}

  IpcPipelineHandler(IpcPipelineHandler &&other) noexcept
      : publisher_(std::move(other.publisher_)) {}

  IpcPipelineHandler &operator=(const IpcPipelineHandler &other) {
    if (this != &other) {
      publisher_ = other.publisher_;
    }
    return *this;
  }

  IpcPipelineHandler &operator=(IpcPipelineHandler &&other) noexcept {
    if (this != &other) {
      publisher_ = std::move(other.publisher_);
    }
    return *this;
  }

  explicit IpcPipelineHandler(std::shared_ptr<PublisherType> publisher)
      : publisher_(std::move(publisher)) {}

  core::Result<schemas::FileInfo> handle(schemas::FileInfo &file) override {
    if (!publisher_) {
      return core::Result<schemas::FileInfo>::Error(
          "Publisher not initialized");
    }

    auto send_result = sendMessage(file);

    if (send_result.is_ok()) {
      return core::Result<schemas::FileInfo>::Ok(file);
    } else {
      spdlog::warn("IPC send failed, but continuing pipeline: {}",
                   send_result.error().what());
      return core::Result<schemas::FileInfo>::Ok(file);
    }
  }

  void await() override {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  core::Result<bool> sendMessage(const schemas::FileInfo &file_info) {
    try {
      spdlog::info("=== Starting sendMessage ===");

      if (!publisher_) {
        spdlog::critical("Publisher is null!");
        return core::Result<bool>::Error("Publisher not initialized");
      }

      auto connections_result = publisher_->update_connections();
      if (connections_result.has_error()) {
        spdlog::warn("No subscribers connected to the publisher");
      }

      auto serialized_data = schemas::FileInfoSerializer::serialize(file_info);
      spdlog::info("Serialized data size: {}", serialized_data.size());

      iox::ImmutableSlice<const uint8_t> data_slice(serialized_data.data(),
                                                    serialized_data.size());

      spdlog::info("Attempting to send slice with {} elements",
                   data_slice.number_of_elements());

      auto send_result = publisher_->send_slice_copy(data_slice);

      if (send_result.has_error()) {
        auto error = send_result.error();
        spdlog::critical("SEND FAILED - Error code: {}",
                         static_cast<int>(error));

        switch (static_cast<int>(error)) {
        case 2:
          spdlog::warn("No subscribers available to receive the message");
          return core::Result<bool>::Ok(true);
        default:
          return core::Result<bool>::Error("Failed to send data");
        }
      }

      spdlog::info("Send successful to {} recipients", send_result.value());
      spdlog::info("=== sendMessage completed ===");
      return core::Result<bool>::Ok(true);

    } catch (const std::exception &e) {
      spdlog::critical("Exception in sendMessage: {}", e.what());
      return core::Result<bool>::Error(e.what());
    }
  }

  bool isConnected() const { return publisher_ != nullptr; }

private:
  std::shared_ptr<PublisherType> publisher_;
};

} // namespace owl

#endif // OWL_IPC_PIPELINE_HANDLER_HPP