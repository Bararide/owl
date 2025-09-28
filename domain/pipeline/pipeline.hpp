#ifndef OWL_VECTORFS_PIPELINE_HPP
#define OWL_VECTORFS_PIPELINE_HPP

#include "event.hpp"
#include "pipeline_handler.hpp"
#include <future>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <type_traits>
#include <vector>

namespace owl::pipeline {

class Pipeline {
public:
  Pipeline() = default;

  Pipeline(Pipeline &&other) noexcept : handlers_(std::move(other.handlers_)) {}

  Pipeline &operator=(Pipeline &&other) noexcept {
    if (this != &other) {
      handlers_ = std::move(other.handlers_);
    }
    return *this;
  }

  Pipeline(const Pipeline &other) = delete;
  Pipeline &operator=(const Pipeline &other) = delete;

  ~Pipeline() = default;

  template <typename Handler>
  void add_handler(std::shared_ptr<Handler> handler) {
    handlers_.push_back(handler);
  }

  template <typename T> core::Result<T> process(const T &data) {
    spdlog::info("Starting pipeline processing with {} handlers",
                 handlers_.size());

    if (handlers_.empty()) {
      return core::Result<T>::Ok(data);
    }

    setup_handler_chain<T>();

    auto promise = std::make_shared<std::promise<core::Result<T>>>();
    auto future = promise->get_future();

    auto last_handler = handlers_.back();
    auto final_handler_id = last_handler->get_event_bus().template Subscribe<T>(
        [promise](const T &final_data) {
          promise->set_value(core::Result<T>::Ok(final_data));
        });

    spdlog::info("Starting processing through event chain");
    auto first_handler = handlers_.front();
    first_handler->await();
    first_handler->get_event_bus().Notify(data);

    auto result = future.get();

    last_handler->get_event_bus().Unsubscribe(final_handler_id);

    cleanup_handler_chain<T>();

    return result;
  }

  template <typename T>
  std::future<core::Result<T>> process_async(const T &data) {
    return std::async(std::launch::async,
                      [this, data]() { return process(data); });
  }

  std::string describe() const {
    std::string result =
        "Pipeline with " + std::to_string(handlers_.size()) + " handlers:\n";

    for (size_t i = 0; i < handlers_.size(); ++i) {
      const auto &handler = *handlers_[i];
      result +=
          "Handler " + std::to_string(i) + ": " + typeid(handler).name() + "\n";
    }

    return result;
  }

  bool empty() const { return handlers_.empty(); }
  size_t size() const { return handlers_.size(); }

private:
  template <typename T> void setup_handler_chain() {
    if (handlers_.size() <= 1)
      return;

    spdlog::info("Setting up handler chain with {} handlers", handlers_.size());

    cleanup_handler_chain<T>();

    for (size_t i = 0; i < handlers_.size() - 1; ++i) {
      auto current_handler = handlers_[i];
      auto next_handler = handlers_[i + 1];

      if (!current_handler || !next_handler)
        continue;

      auto subscription_id =
          current_handler->get_event_bus().template Subscribe<T>(
              [next_handler](const T &data) {
                spdlog::info("Passing data to next handler");
                next_handler->await();
                auto result = next_handler->handle(data);
                if (result.is_ok()) {
                  next_handler->get_event_bus().Notify(result.value());
                }
              });

      chain_subscriptions_.push_back({current_handler.get(), subscription_id});
    }
  }

  template <typename T> void cleanup_handler_chain() {
    for (const auto &[handler, subscription_id] : chain_subscriptions_) {
      handler->get_event_bus().Unsubscribe(subscription_id);
    }
    chain_subscriptions_.clear();
  }

private:
  std::vector<std::shared_ptr<IHandler>> handlers_;
  std::vector<std::pair<IHandler *, core::Event::handlerID>>
      chain_subscriptions_;
};

} // namespace owl::pipeline

#endif // OWL_VECTORFS_PIPELINE_HPP