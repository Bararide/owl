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

    spdlog::critical("IpcPipelineHandler");

    return sendMessage(file).and_then(
        [&file]() -> core::Result<schemas::FileInfo> {
          return core::Result<schemas::FileInfo>::Ok(file);
        });
  }

  void await() override {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  core::Result<bool> sendMessage(const schemas::FileInfo &file_info) {
    try {
      auto serialized_data = schemas::FileInfoSerializer::serialize(file_info);

      iox::ImmutableSlice<const uint8_t> data_slice(serialized_data.data(),
                                                    serialized_data.size());

      auto send_result = publisher_->send_slice_copy(data_slice);

      if (send_result.has_error()) {
        return core::Result<bool>::Error("Failed to send data");
      }

      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  bool isConnected() const { return publisher_ != nullptr; }

private:
  std::shared_ptr<PublisherType> publisher_;
};

} // namespace owl

#endif // OWL_IPC_PIPELINE_HANDLER_HPP