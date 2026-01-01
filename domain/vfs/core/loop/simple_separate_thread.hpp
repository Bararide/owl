#ifndef OWL_VFS_CORE_SIMPLE_LOOP_RUNNER_HPP
#define OWL_VFS_CORE_SIMPLE_LOOP_RUNNER_HPP

#include "thread.hpp"
#include "runner.hpp"

namespace owl {

template <class TLoop>
class SimpleSeparateThreadLoopRunner final
    : public LoopRunner<SimpleSeparateThreadLoopRunner<TLoop>, TLoop> {
public:
  explicit SimpleSeparateThreadLoopRunner(std::shared_ptr<TLoop> loop)
      : loop_(std::move(loop)) {}

  void start() {
    loop_->setIsActive(true);
    createAndStartThread();
  }

  void start(std::string_view thread_name,
             std::optional<uint64_t> cpu_id = std::nullopt) {
    start();
    setThreadNameAndAffinity(active_thread_.get(), thread_name, cpu_id);
  }

  void stop() {
    if (active_thread_ == nullptr) {
      return;
    }

    loop_->setIsActive(false);
    active_thread_->join();
    active_thread_ = nullptr;
  }

  std::shared_ptr<TLoop> loop() { return loop_; }
  const std::shared_ptr<TLoop> &loop() const { return loop_; }

  ~SimpleSeparateThreadLoopRunner() { stop(); }

private:
  std::shared_ptr<TLoop> loop_;
  std::unique_ptr<std::thread> active_thread_{nullptr};

  void createAndStartThread() {
    active_thread_ = std::make_unique<std::thread>([this] {
      loop_->start();
      while (loop_->getIsActive()) {
        loop_->update();
      }
      loop_->stop();
    });
  }
};

template <class TLoop>
class ManagedSimpleSeparateThreadLoopRunner final
    : public LoopRunner<ManagedSimpleSeparateThreadLoopRunner<TLoop>, TLoop> {
public:
  ManagedSimpleSeparateThreadLoopRunner() : loop_{} {}

  explicit ManagedSimpleSeparateThreadLoopRunner(TLoop &&loop)
      : loop_{std::move(loop)} {}

  void start() {
    loop_.setIsActive(true);
    active_thread_ = std::make_unique<std::thread>([&] {
      loop_.start();
      while (loop_.getIsActive()) {
        loop_.update();
      }
      loop_.stop();
    });
  }

  void start(std::string_view thread_name,
             std::optional<uint64_t> cpu_id = std::nullopt) {
    start();
    setThreadNameAndAffinity(active_thread_.get(), thread_name, cpu_id);
  }

  void stop() {
    if (active_thread_ == nullptr) {
      return;
    }

    loop_.setIsActive(false);
    active_thread_->join();
    active_thread_ = nullptr;
  }

  TLoop &loop() { return loop_; }
  const TLoop &loop() const { return loop_; }

  void setMaxFifoPriority() {
    if (active_thread_ != nullptr) {
      sched_param param{};
      param.sched_priority = sched_get_priority_max(SCHED_FIFO);
      pthread_setschedparam(active_thread_->native_handle(), SCHED_FIFO,
                            &param);
    }
  }

  ~ManagedSimpleSeparateThreadLoopRunner() { stop(); }

private:
  TLoop loop_;
  std::unique_ptr<std::thread> active_thread_{nullptr};
};

} // namespace owl

#endif // OWL_VFS_CORE_SIMPLE_LOOP_RUNNER_HPP
