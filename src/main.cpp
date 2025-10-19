#include "instance/instance.hpp"
#include "network/api.hpp"
#include "parsers/parser_pdf.hpp"
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

    core::measure::Measure::start();

    owl::instance::VFSInstance<embedded::FastTextEmbedder,
                               owl::compression::Compressor>::
        initialize(std::move(fasttext_model_path));

    core::measure::Measure::end();
    core::measure::Measure::result<std::chrono::milliseconds>(
        "VectorFS initialized with compression in {} ms");

    core::measure::Measure::start();
    auto &vectorfs =
        owl::instance::VFSInstance<embedded::FastTextEmbedder,
                                   owl::compression::Compressor>::getInstance();
    core::measure::Measure::end();
    core::measure::Measure::result<std::chrono::milliseconds>(
        "VectorFS with compression loaded in {} ms");

    spdlog::info("Embedder: {}", vectorfs.get_embedder_info());

    core::measure::Measure::start();
    vectorfs.test_semantic_search();
    core::measure::Measure::end();
    core::measure::Measure::result<std::chrono::milliseconds>(
        "Semantic search test completed in {} ms");

    core::measure::Measure::start();
    vectorfs.test_markov_model();
    core::measure::Measure::end();
    core::measure::Measure::result<std::chrono::milliseconds>(
        "Markov search test completed in {} ms");

    core::measure::Measure::start();
    vectorfs.test_container();
    core::measure::Measure::end();
    core::measure::Measure::result<std::chrono::milliseconds>(
        "Container test completed in {} ms");

    try {
      core::measure::Measure::start();
      auto embedding = vectorfs.get_embedding("test sentence");
      core::measure::Measure::end();
      auto embed_duration =
          core::measure::Measure::duration<std::chrono::microseconds>();
      spdlog::info(
          "Embedding generated successfully, dimension: {}, time: {} μs",
          embedding.size(), embed_duration.count());
    } catch (const std::exception &e) {
      spdlog::warn("Embedding test failed: {}", e.what());
      core::measure::Measure::cancel();
    }

    auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
    if (!shm_manager.initialize()) {
      spdlog::warn("Failed to initialize shared memory in main process");
    }

    core::measure::Measure::start();

    pid_t http_pid = fork();
    if (http_pid == 0) {
      core::measure::Measure::reset();

      spdlog::info("Starting HTTP server in child process (PID: {})...",
                   getpid());
      try {
        auto http_start = std::chrono::high_resolution_clock::now();

        owl::network::VectorFSApi<embedded::FastTextEmbedder>::init();
        owl::network::VectorFSApi<embedded::FastTextEmbedder>::run();

        auto http_end = std::chrono::high_resolution_clock::now();
        auto http_duration = std::chrono::duration_cast<std::chrono::seconds>(
            http_end - http_start);
        spdlog::info("HTTP server ran for {} seconds", http_duration.count());
      } catch (const std::exception &e) {
        spdlog::error("HTTP server error: {}", e.what());
        exit(EXIT_FAILURE);
      }
      exit(0);
    } else if (http_pid > 0) {
      spdlog::info("Starting FUSE in parent process (PID: {})...", getpid());
      std::this_thread::sleep_for(std::chrono::seconds(2));

      auto fuse_start = std::chrono::high_resolution_clock::now();
      int result = vectorfs.initialize_fuse(argc, argv);
      auto fuse_end = std::chrono::high_resolution_clock::now();
      auto fuse_duration = std::chrono::duration_cast<std::chrono::seconds>(
          fuse_end - fuse_start);

      core::measure::Measure::end();
      auto total_duration =
          core::measure::Measure::duration<std::chrono::seconds>();

      spdlog::info("FUSE exited with code: {}, ran for {} seconds", result,
                   fuse_duration.count());
      spdlog::info("Total application runtime: {} seconds",
                   total_duration.count());

      spdlog::info("Stopping HTTP server...");
      if (kill(http_pid, SIGTERM) == 0) {
        int status;
        waitpid(http_pid, &status, 0);
        spdlog::info("HTTP server terminated with status: {}", status);
      } else {
        spdlog::warn("Failed to terminate HTTP server gracefully");
      }

      owl::instance::VFSInstance<embedded::FastTextEmbedder>::shutdown();
      spdlog::info("VectorFS shutdown complete");
      return result;
    } else {
      spdlog::error("Failed to fork process for HTTP server");
      return EXIT_FAILURE;
    }
  } catch (const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    core::measure::Measure::cancel();
    try {
      owl::instance::VFSInstance<embedded::FastTextEmbedder>::shutdown();
    } catch (...) {
    }
    return EXIT_FAILURE;
  }
}