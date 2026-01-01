#ifndef OWL_VFS_MQ_MQ_LOOP
#define OWL_VFS_MQ_MQ_LOOP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>

namespace owl {

class MQLoop {
public:
  using MessageHandler = std::function<void(
      const std::string &, const std::string &, const nlohmann::json &)>;

  explicit MQLoop(MessageHandler handler) : handler_(std::move(handler)) {}

  MQLoop(MQLoop &&other) noexcept
      : handler_(std::move(other.handler_)),
        is_active_(other.is_active_.load()) {}

  MQLoop &operator=(MQLoop &&other) noexcept {
    handler_ = std::move(other.handler_);
    is_active_ = other.is_active_.load();
    return *this;
  }

  MQLoop(const MQLoop &) = delete;
  MQLoop &operator=(const MQLoop &) = delete;

  void start() { is_active_ = true; }

  void stop() { is_active_ = false; }

  void update() {
    if (is_active_) {
      // handler_(verb, path, msg)
    }
  }

  void setIsActive(bool active) { is_active_ = active; }
  bool getIsActive() const { return is_active_; }

  void sendMessage(const std::string &queue, const std::string &verb,
                   const std::string &path, const nlohmann::json &body) {}

private:
  MessageHandler handler_;
  std::atomic<bool> is_active_{false};
};

} // namespace owl

#endif // OWL_VFS_MQ_MQ_LOOP