#ifndef VFS_IPC_HPP
#define VFS_IPC_HPP

#include "utils/shared_types.hpp"
#include "vectorfs.hpp"
#include <iceoryx2-cxx/include/iox2/allocation_strategy.hpp>

namespace vfs::ipc {

class VectorFSIpcService {
private:
  vfs::vectorfs::VectorFS &vectorfs_;
  iox2::Runtime runtime_;
  iox2::Publisher<FileCreateRequest> create_pub_;
  iox2::Subscriber<FileCreateRequest> create_sub_;
  iox2::Publisher<SyncResponse> response_pub_;

  std::thread ipc_thread_;
  std::atomic<bool> running_{false};

public:
  VectorFSIpcService(vfs::vectorfs::VectorFS &vfs)
      : vectorfs_(vfs), runtime_("vectorfs_service"),
        create_pub_(runtime_, "FileCreateRequests"),
        create_sub_(runtime_, "FileCreateRequests"),
        response_pub_(runtime_, "SyncResponses") {}

  void start() {
    running_ = true;
    ipc_thread_ = std::thread([this]() {
      while (running_) {
        process_requests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    });
  }

  void stop() {
    running_ = false;
    if (ipc_thread_.joinable()) {
      ipc_thread_.join();
    }
  }

  void process_requests() {
    if (auto request = create_sub_.receive()) {
      spdlog::info("Received file creation request: {}", request->path);

      try {
        struct fuse_file_info fi {};
        int result = vectorfs_.create(request->path.c_str(), request->mode, &fi,
                                      request->content);

        SyncResponse response;
        if (result == 0) {
          response.success = true;
          response.message = "File created successfully";
        } else {
          response.success = false;
          response.message = "Failed to create file: " + std::to_string(result);
        }

        response_pub_.publish(response);

      } catch (const std::exception &e) {
        SyncResponse response;
        response.success = false;
        response.message = "Exception: " + std::string(e.what());
        response_pub_.publish(response);
      }
    }
  }

  void notify_file_created(const std::string &path, const std::string &content,
                           mode_t mode) {
    FileCreateRequest request;
    request.path = path;
    request.content = content;
    request.mode = mode;
    create_pub_.publish(request);
  }
};

} // namespace vfs::ipc

#endif // VFS_IPC_HPP
