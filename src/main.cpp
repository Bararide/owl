#include "instance/instance.hpp"
#include "network/api.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

std::atomic<bool> running{true};

void signal_handler(int signal) {
  spdlog::info("Received signal {}, shutting down...", signal);
  running = false;
}

int main(int argc, char *argv[]) {
  try {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting VectorFS...");

    const std::string fasttext_model_path =
        "/home/bararide/code/models/crawl-300d-2M-subword/"
        "crawl-300d-2M-subword.bin";

    auto start = std::chrono::high_resolution_clock::now();
    vfs::instance::VFSInstance::initialize(fasttext_model_path);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("VectorFS initialized in {} ms", duration);

    start = std::chrono::high_resolution_clock::now();
    auto &vectorfs = vfs::instance::VFSInstance::getInstance();
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("VectorFS loaded in {} ms", duration);

    start = std::chrono::high_resolution_clock::now();
    vectorfs.test_semantic_search();
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("Semantic search test completed in {} ms", duration);

    pid_t http_pid = fork();

    if (http_pid == 0) {
      spdlog::info("Starting HTTP server in child process (PID: {})...",
                   getpid());
      vfs::network::VectorFSApi::init();
      vfs::network::VectorFSApi::run();
      exit(0);
    } else if (http_pid > 0) {
      spdlog::info("Starting FUSE in parent process (PID: {})...", getpid());

      std::this_thread::sleep_for(std::chrono::seconds(2));

      int result = vectorfs.initialize_fuse(argc, argv);

      spdlog::info("FUSE exited with code: {}, stopping HTTP server...",
                   result);
      kill(http_pid, SIGTERM);

      int status;
      waitpid(http_pid, &status, 0);

      vfs::instance::VFSInstance::shutdown();
      spdlog::info("VectorFS shutdown complete");
      return result;
    } else {
      spdlog::error("Failed to fork process for HTTP server");
      return EXIT_FAILURE;
    }

  } catch (const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    return EXIT_FAILURE;
  }
}