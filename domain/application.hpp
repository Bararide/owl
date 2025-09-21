#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "instance/instance.hpp"
#include "network/api.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace owl::app {

std::atomic<bool> running{true};

void signal_handler(int signal) {
  spdlog::info("Received signal {}, shutting down...", signal);
  running = false;
}

class VectorFSApplication {
public:
  VectorFSApplication(int argc, char *argv[]) : argc_(argc), argv_(argv) {}

  int run() {
    try {
      initialize_signals();
      initialize_logging();
      initialize_application();

      return run_services();
    } catch (const std::exception &e) {
      handle_fatal_error(e);
      return EXIT_FAILURE;
    }
  }

private:
  void initialize_signals() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
  }

  void initialize_logging() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting VectorFS...");
  }

  void initialize_application() {
    constexpr const char *fasttext_model_path =
        "/home/bararide/code/models/crawl-300d-2M-subword/"
        "crawl-300d-2M-subword.bin";

    measure_duration("VectorFS initialized", [&] {
      owl::instance::VFSInstance<owl::embedded::FastTextEmbedder>::initialize(
          fasttext_model_path);
    });

    measure_duration("VectorFS loaded", [&] {
      vectorfs_ = &owl::instance::VFSInstance<
          owl::embedded::FastTextEmbedder>::getInstance();
    });

    spdlog::info("Embedder: {}", vectorfs_->get_embedder_info());

    run_tests();
    initialize_shared_memory();
  }

  void run_tests() {
    measure_duration("Semantic search test",
                     [&] { vectorfs_->test_semantic_search(); });

    measure_duration("Markov search test",
                     [&] { vectorfs_->test_markov_model(); });

    test_embedding();
  }

  void test_embedding() {
    try {
      std::vector<float> embedding;
      auto duration = measure_duration<std::chrono::microseconds>(
          [&] { embedding = vectorfs_->get_embedding("test sentence"); });

      spdlog::info(
          "Embedding generated successfully, dimension: {}, time: {} μs",
          embedding.size(), duration.count());
    } catch (const std::exception &e) {
      spdlog::warn("Embedding test failed: {}", e.what());
    }
  }

  void initialize_shared_memory() {
    auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
    if (!shm_manager.initialize()) {
      spdlog::warn("Failed to initialize shared memory in main process");
    }
  }

  int run_services() {
    pid_t http_pid = fork_http_server();
    if (http_pid == 0) {
      run_http_server();
    } else if (http_pid > 0) {
      return run_fuse_and_cleanup(http_pid);
    } else {
      spdlog::error("Failed to fork process for HTTP server");
      return EXIT_FAILURE;
    }
  }

  pid_t fork_http_server() { return fork(); }

  void run_http_server() {
    core::measure::Measure::reset();
    spdlog::info("Starting HTTP server in child process (PID: {})...",
                 getpid());

    try {
      auto start_time = std::chrono::high_resolution_clock::now();

      owl::network::VectorFSApi<owl::embedded::FastTextEmbedder>::init();
      owl::network::VectorFSApi<owl::embedded::FastTextEmbedder>::run();

      auto duration = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::high_resolution_clock::now() - start_time);
      spdlog::info("HTTP server ran for {} seconds", duration.count());

    } catch (const std::exception &e) {
      spdlog::error("HTTP server error: {}", e.what());
      exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
  }

  int run_fuse_and_cleanup(pid_t http_pid) {
    spdlog::info("Starting FUSE in parent process (PID: {})...", getpid());
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto [fuse_result, fuse_duration] = run_fuse();
    auto total_duration =
        core::measure::Measure::duration<std::chrono::seconds>();

    log_results(fuse_result, fuse_duration, total_duration);
    cleanup(http_pid);

    return fuse_result;
  }

  std::pair<int, std::chrono::seconds> run_fuse() {
    auto start_time = std::chrono::high_resolution_clock::now();
    int result = vectorfs_->initialize_fuse(argc_, argv_);
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - start_time);

    return {result, duration};
  }

  void log_results(int fuse_result, std::chrono::seconds fuse_duration,
                   std::chrono::seconds total_duration) {
    spdlog::info("FUSE exited with code: {}, ran for {} seconds", fuse_result,
                 fuse_duration.count());
    spdlog::info("Total application runtime: {} seconds",
                 total_duration.count());
  }

  void cleanup(pid_t http_pid) {
    stop_http_server(http_pid);
    shutdown_application();
  }

  void stop_http_server(pid_t http_pid) {
    spdlog::info("Stopping HTTP server...");

    if (kill(http_pid, SIGTERM) == 0) {
      int status;
      waitpid(http_pid, &status, 0);
      spdlog::info("HTTP server terminated with status: {}", status);
    } else {
      spdlog::warn("Failed to terminate HTTP server gracefully");
    }
  }

  void shutdown_application() {
    owl::instance::VFSInstance<owl::embedded::FastTextEmbedder>::shutdown();
    spdlog::info("VectorFS shutdown complete");
  }

  void handle_fatal_error(const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    core::measure::Measure::cancel();

    try {
      owl::instance::VFSInstance<owl::embedded::FastTextEmbedder>::shutdown();
    } catch (...) {
      spdlog::warn("Error during shutdown");
    }
  }

  template <typename Duration = std::chrono::milliseconds, typename Func>
  Duration measure_duration(const std::string &message, Func &&func) {
    core::measure::Measure::start();
    std::forward<Func>(func)();
    core::measure::Measure::end();

    auto duration = core::measure::Measure::duration<Duration>();
    spdlog::info("{} in {} {}", message, duration.count(),
                 typeid(Duration) == typeid(std::chrono::milliseconds) ? "ms"
                                                                       : "μs");

    return duration;
  }

  template <typename Duration = std::chrono::milliseconds, typename Func>
  Duration measure_duration(Func &&func) {
    core::measure::Measure::start();
    std::forward<Func>(func)();
    core::measure::Measure::end();

    return core::measure::Measure::duration<Duration>();
  }

private:
  int argc_;
  char **argv_;
  owl::instance::VFSInstance<owl::embedded::FastTextEmbedder> *vectorfs_{
      nullptr};
};

} // namespace owl::app

#endif // APPLICATION_HPP
