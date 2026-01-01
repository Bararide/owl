#ifndef OWL_VFS_MQ_MQ_LOOP
#define OWL_VFS_MQ_MQ_LOOP

#include "zeromq_loop.hpp"

namespace owl {

class MQLoop {
public:
  using MessageHandler = std::function<void(
      const std::string &, const std::string &, const nlohmann::json &)>;

  explicit MQLoop(MessageHandler handler)
      : zeromq_loop_(std::make_unique<ZeroMQLoop>(std::move(handler))) {}

  void start() {
    if (zeromq_loop_) {
      zeromq_loop_->start();
    }
  }

  void stop() {
    if (zeromq_loop_) {
      zeromq_loop_->stop();
    }
  }

  void update() {
    if (zeromq_loop_) {
      zeromq_loop_->update();
    }
  }

  void setIsActive(bool active) {
    if (zeromq_loop_) {
      zeromq_loop_->setIsActive(active);
    }
  }

  bool getIsActive() const {
    return zeromq_loop_ ? zeromq_loop_->getIsActive() : false;
  }

  void sendResponse(const std::string &request_id, bool success,
                    const nlohmann::json &data) {
    if (zeromq_loop_) {
      zeromq_loop_->sendResponse(request_id, success, data);
    }
  }

  void sendMessage(const std::string &queue, const std::string &verb,
                   const std::string &path, const nlohmann::json &body) {}

private:
  std::unique_ptr<ZeroMQLoop> zeromq_loop_;
};

} // namespace owl

#endif // OWL_VFS_MQ_MQ_LOOP