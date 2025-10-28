#include "api/api.hpp"
#include "instance/instance.hpp"
#include <atomic>
#include <chrono>
#include <clocale>
#include <csignal>
#include <execinfo.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

std::atomic<bool> running{true};

void print_backtrace() {
  void *buffer[100];
  int size = backtrace(buffer, 100);
  char **symbols = backtrace_symbols(buffer, size);

  spdlog::error("=== BACKTRACE ===");
  for (int i = 0; i < size; i++) {
    spdlog::error("{}: {}", i, symbols[i]);
  }
  spdlog::error("=================");

  free(symbols);
}

void signal_handler(int signal) {
  spdlog::error("Received signal {} in PID {}", signal, getpid());
  print_backtrace();

  if (signal == SIGSEGV || signal == SIGABRT) {
    spdlog::error("Critical signal received, exiting...");
    _exit(1);
  }

  running = false;
}

void setup_safe_locale() {
  std::setlocale(LC_ALL, "C");

  if (std::setlocale(LC_ALL, "C") != nullptr) {
    spdlog::info("Locale set to C");
  } else {
    spdlog::warn("Failed to set C locale, using default");
  }

  try {
    std::locale::global(std::locale::classic());
  } catch (const std::exception &e) {
    spdlog::warn("Failed to set C++ locale: {}", e.what());
  }
}

int main(int argc, char *argv[]) {
  try {
    setup_safe_locale();

    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGILL, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    spdlog::set_level(spdlog::level::debug);
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

    auto &vectorfs =
        owl::instance::VFSInstance<embedded::FastTextEmbedder,
                                   owl::compression::Compressor>::getInstance();

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

    // core::measure::Measure::start();
    // vectorfs.test_container();
    // core::measure::Measure::end();
    // core::measure::Measure::result<std::chrono::milliseconds>(
    //     "Container test completed in {} ms");

    core::measure::Measure::start();

    pid_t http_pid = fork();
    if (http_pid == 0) {
      spdlog::info("=== HTTP PROCESS STARTING ===");
      spdlog::info("PID: {}, PPID: {}", getpid(), getppid());

      setup_safe_locale();

      try {
        spdlog::info("Initializing Pistache...");

        std::set_terminate([]() {
          spdlog::error("=== TERMINATE CALLED IN HTTP PROCESS ===");
          print_backtrace();
          _exit(1);
        });

        owl::api::VectorFSApi<embedded::FastTextEmbedder> api;
        api.init();
        spdlog::info("Pistache initialized successfully");

        spdlog::info("Starting Pistache event loop...");
        api.run();

        spdlog::info("Pistache event loop exited normally");

      } catch (const std::exception &e) {
        spdlog::error("HTTP server exception: {}", e.what());
        print_backtrace();
        _exit(EXIT_FAILURE);
      }

      spdlog::info("HTTP server exited normally");
      _exit(0);
    } else if (http_pid > 0) {
      spdlog::info("Starting FUSE in parent process (PID: {}), HTTP PID: {}",
                   getpid(), http_pid);

      std::thread monitor_thread([http_pid]() {
        spdlog::info("HTTP monitor thread started");
        int status;
        while (running) {
          pid_t result = waitpid(http_pid, &status, WNOHANG);
          if (result == http_pid) {
            if (WIFEXITED(status)) {
              spdlog::error("HTTP process EXITED with status: {}",
                            WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
              spdlog::error("HTTP process KILLED by signal: {}",
                            WTERMSIG(status));
            }
            running = false;
            break;
          } else if (result == -1) {
            spdlog::error("waitpid error on HTTP process");
            running = false;
            break;
          }
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        spdlog::info("HTTP monitor thread exiting");
      });
      monitor_thread.detach();

      std::this_thread::sleep_for(std::chrono::seconds(2));

      spdlog::info("Mounting FUSE filesystem...");

      auto fuse_start = std::chrono::high_resolution_clock::now();

      int result = vectorfs.initialize_fuse(argc, argv);

      auto fuse_end = std::chrono::high_resolution_clock::now();

      core::measure::Measure::end();
      auto total_duration =
          core::measure::Measure::duration<std::chrono::seconds>();

      spdlog::info("FUSE exited with code: {}, total runtime: {} seconds",
                   result, total_duration.count());

      running = false;

      std::this_thread::sleep_for(std::chrono::seconds(1));

      spdlog::info("Stopping HTTP server...");
      if (kill(http_pid, SIGTERM) == 0) {
        int status;
        waitpid(http_pid, &status, 0);
        spdlog::info("HTTP server terminated");
      }

      owl::instance::VFSInstance<embedded::FastTextEmbedder,
                                 owl::compression::Compressor>::shutdown();
      spdlog::info("VectorFS shutdown complete");
      return result;
    } else {
      spdlog::error("Failed to fork process for HTTP server");
      return EXIT_FAILURE;
    }
  } catch (const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    print_backtrace();
    return EXIT_FAILURE;
  }
}