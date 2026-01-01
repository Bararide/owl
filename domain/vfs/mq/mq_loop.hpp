#ifndef OWL_VFS_LOOP_MQ_LOOP
#define OWL_VFS_LOOP_MQ_LOOP

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace owl {

class MQLoop {
public:
    using MessageHandler = std::function<void(const std::string&, 
                                              const std::string&, 
                                              const nlohmann::json&)>;
    
    MQLoop(MessageHandler handler) : handler_(std::move(handler)) {}
    
    void start() {
        // connection_ = std::make_unique<amqp::Connection>("localhost");
        // channel_ = connection_->createChannel();
        // channel_->declareQueue("vfs_events");
    }
    
    void stop() {
        // if (channel_) channel_->close();
        // if (connection_) connection_->close();
    }
    
    void update() {
        // while (auto message = channel_->get("vfs_events")) {
        //     handler_(message->verb(), message->path(), message->body());
        // }
        
        // channel_->consume("vfs_events", [this](auto msg) {
        //     handler_(msg.verb(), msg.path(), msg.body());
        // });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void setIsActive(bool active) { is_active_ = active; }
    bool getIsActive() const { return is_active_; }
    
    void sendMessage(const std::string& queue, 
                     const std::string& verb,
                     const std::string& path,
                     const nlohmann::json& body) {
        // channel_->publish(queue, verb, path, body.dump());
    }

private:
    MessageHandler handler_;
    std::atomic<bool> is_active_{false};
    // std::unique_ptr<amqp::Connection> connection_;
    // std::unique_ptr<amqp::Channel> channel_;
};

} // namespace owl

#endif // OWL_VFS_LOOP_MQ_LOOP