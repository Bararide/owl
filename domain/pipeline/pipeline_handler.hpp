#ifndef OWL_VECTORFS_PIPELINE_HANDLER_HPP
#define OWL_VECTORFS_PIPELINE_HANDLER_HPP

#include <infrastructure/concepts.hpp>
#include <infrastructure/event.hpp>
#include <infrastructure/result.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace owl::pipeline {

class IHandler {
public:
  virtual ~IHandler() = default;
  virtual void await() = 0;
  virtual core::Event &get_event_bus() = 0;

  template <typename T> core::Result<T> handle(const T &data) {
    return do_handle(data);
  }

  void compare(const std::vector<std::shared_ptr<IHandler>> &data,
               std::string &result) {
    return doCompare(data, result);
  }

private:
  virtual core::Result<std::vector<int>>
  do_handle(const std::vector<int> &data) = 0;
  virtual void doCompare(const std::vector<std::shared_ptr<IHandler>> &data,
                         std::string &result) = 0;
};

template <typename Derived, typename Input = std::vector<int>,
          typename Output = std::vector<int>>
class PipelineHandler : public IHandler {
public:
  PipelineHandler() = default;

  core::Event &get_event_bus() override { return event_bus_; }

  core::Result<Output> handle(const Input &data) {
    if constexpr (requires(Derived d) { d.handle(data); }) {
      return static_cast<Derived *>(this)->handle(data);
    } else {
      return core::Result<Output>::Ok(data);
    }
  }

  void await() override {
    if constexpr (requires(Derived d) { d.await(); }) {
      static_cast<Derived *>(this)->await();
    } else if constexpr (requires(Derived d) { d.await_impl(); }) {
      static_cast<Derived *>(this)->await_impl();
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

protected:
  ~PipelineHandler() = default;

private:
  core::Event event_bus_;

  core::Result<std::vector<int>>
  do_handle(const std::vector<int> &data) override {
    return handle(data);
  }

  void doCompare(const std::vector<std::shared_ptr<IHandler>> &data,
                 std::string &result) override {
    result = "PipelineHandler<" + std::string(typeid(Derived).name()) + ">";
  }
};

} // namespace owl::pipeline

#endif // OWL_VECTORFS_PIPELINE_HANDLER_HPP