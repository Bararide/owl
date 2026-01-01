#ifndef OWL_VFS_LOOP_EVENT_LOOP
#define OWL_VFS_LOOP_EVENT_LOOP

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <functional>
#include <memory>

namespace owl {

class EventLoop {
public:
  EventLoop(size_t thread_count = std::thread::hardware_concurrency())
      : thread_count_(thread_count),
        io_context_(std::make_shared<boost::asio::io_context>()),
        work_guard_(boost::asio::make_work_guard(*io_context_)),
        pool_(thread_count) {}

  ~EventLoop() { stop(); }

  void start() {
    for (size_t i = 0; i < thread_count_; ++i) {
      boost::asio::post(pool_, [this]() { io_context_->run(); });
    }
  }

  void stop() {
    work_guard_.reset();
    io_context_->stop();
    pool_.join();
  }

  template <typename F> void post(F &&task) {
    boost::asio::post(*io_context_, std::forward<F>(task));
  }

  template <typename F, typename... Args> void post(F &&f, Args &&...args) {
    post(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  }

  boost::asio::io_context &get_io_context() { return *io_context_; }

private:
  size_t thread_count_;
  std::shared_ptr<boost::asio::io_context> io_context_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;
  boost::asio::thread_pool pool_;
};

} // namespace owl

#endif // OWL_VFS_LOOP_EVENT_LOOP