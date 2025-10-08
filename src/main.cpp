#include "application.hpp"
#include "basement/basement.hpp"
#include "network/capnproto.hpp"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <iox2/client.hpp>
#include <list>
#include <pipeline/pipeline.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::atomic<bool> g_shutdown_requested{false};

std::string getHomeDirectory() {
  const char *home = getenv("HOME");
  if (home)
    return home;

  struct passwd *pw = getpwuid(getuid());
  return pw ? pw->pw_dir : "";
}

void signalHandler(int signal) {
  spdlog::info("Received signal: {}, initiating shutdown...", signal);
  g_shutdown_requested.store(true);
}

void setupSignalHandlers() {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  std::signal(SIGQUIT, signalHandler);
  std::signal(SIGPIPE, SIG_IGN);
}

void printUsage(const char *program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout
      << "  --server ADDRESS    Set server address (default: 127.0.0.1:5346)"
      << std::endl;
  std::cout << "  --model PATH        Path to embedder model file" << std::endl;
  std::cout << "  --quantization      Enable quantization" << std::endl;
  std::cout << "  --mount POINT       FUSE mount point (default: /mnt/vectorfs)"
            << std::endl;
  std::cout << "  --help              Show this help message" << std::endl;
}

void testServerConnection(owl::app::Application<> &app) {
  spdlog::info("Testing server connection...");
  auto status_result = app.getServerStatus();
  if (status_result.is_ok()) {
    spdlog::info("Server is running on: {}", app.getServerAddress());
    app.createClient();
  } else {
    spdlog::error("Server is not running: {}", status_result.error().what());
  }
}

bool createMountPoint(const std::string &mount_point) {
  try {
    if (!std::filesystem::exists(mount_point)) {
      spdlog::info("Creating mount point: {}", mount_point);
      return std::filesystem::create_directories(mount_point);
    }
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Failed to create mount point: {}", e.what());
    return false;
  }
}

pid_t startFuseProcess(const std::string &mount_point) {
  if (!createMountPoint(mount_point)) {
    return -1;
  }

  std::vector<std::string> fuse_args = {"vectorfs", "-f", "-s", mount_point};

  std::vector<char *> fuse_argv;
  for (auto &arg : fuse_args) {
    fuse_argv.push_back(const_cast<char *>(arg.c_str()));
  }
  fuse_argv.push_back(nullptr);
  int fuse_argc = fuse_argv.size() - 1;

  pid_t pid = fork();
  if (pid == 0) {
    spdlog::info("Starting FUSE process: {}", getpid());

    try {
      owl::Basement basement;
      auto init_result = basement.init();
      if (!init_result.is_ok()) {
        spdlog::error("Failed to initialize basement");
        exit(EXIT_FAILURE);
      }

      int fuse_result = basement.initialize_fuse(fuse_argc, fuse_argv.data());
      spdlog::info("FUSE process exiting with code: {}", fuse_result);
      exit(fuse_result);

    } catch (const std::exception &e) {
      spdlog::error("FUSE process exception: {}", e.what());
      exit(EXIT_FAILURE);
    }
  }

  return pid;
}

bool isProcessAlive(pid_t pid) {
  if (pid <= 0)
    return false;
  int status;
  return waitpid(pid, &status, WNOHANG) == 0;
}

void stopFuseProcess(pid_t pid) {
  if (pid > 0 && isProcessAlive(pid)) {
    spdlog::info("Stopping FUSE process: {}", pid);
    kill(pid, SIGTERM);

    int status;
    int count = 0;
    while (waitpid(pid, &status, WNOHANG) == 0 && count < 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      count++;
    }

    if (waitpid(pid, &status, WNOHANG) == 0) {
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
    }
  }
}

int main(int argc, char *argv[]) {
  setupSignalHandlers();

  std::string server_address = "127.0.0.1:5346";
  std::string model_path = owl::kEmbedderModel;
  std::string mount_point = getHomeDirectory() + "/my_fuse_mount";
  bool use_quantization = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--server" && i + 1 < argc) {
      server_address = argv[++i];
    } else if (arg == "--model" && i + 1 < argc) {
      model_path = argv[++i];
    } else if (arg == "--mount" && i + 1 < argc) {
      mount_point = argv[++i];
    } else if (arg == "--quantization") {
      use_quantization = true;
    } else if (arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else {
      spdlog::warn("Unknown argument: {}", arg);
      printUsage(argv[0]);
      return 1;
    }
  }

  spdlog::info("Starting VectorFS Application");
  spdlog::info("Configuration:");
  spdlog::info("  Server address: {}", server_address);
  spdlog::info("  Model path: {}", model_path);
  spdlog::info("  Mount point: {}", mount_point);
  spdlog::info("  Quantization: {}", use_quantization ? "enabled" : "disabled");

  pid_t fuse_pid = startFuseProcess(mount_point);
  if (fuse_pid <= 0) {
    spdlog::error("Failed to start FUSE process");
    return 1;
  }

  spdlog::info("Waiting for FUSE process to initialize...");
  std::this_thread::sleep_for(std::chrono::seconds(3));

  if (!isProcessAlive(fuse_pid)) {
    spdlog::error("FUSE process failed to start");
    return 1;
  }

  spdlog::info("FUSE process started successfully with PID: {}", fuse_pid);

  int exit_code = 0;
  int restart_count = 0;
  const int max_restarts = 5;
  pid_t current_fuse_pid = fuse_pid;

  while (!g_shutdown_requested && restart_count < max_restarts) {
    if (!isProcessAlive(current_fuse_pid)) {
      spdlog::warn("FUSE process died, restarting...");
      stopFuseProcess(current_fuse_pid);

      if (!g_shutdown_requested && restart_count < max_restarts) {
        current_fuse_pid = startFuseProcess(mount_point);
        if (current_fuse_pid > 0) {
          std::this_thread::sleep_for(std::chrono::seconds(3));
          if (isProcessAlive(current_fuse_pid)) {
            restart_count++;
            spdlog::warn("Restarted FUSE process (attempt {}/{})",
                         restart_count, max_restarts);
            continue;
          }
        }
      }
    }

    try {
      auto app = owl::app::Application<>(argc, argv);

      spdlog::info("Initializing application...");
      auto init_result = app.run(model_path, use_quantization);

      if (!init_result.is_ok()) {
        spdlog::error("Failed to initialize application: {}",
                      init_result.error().what());
        if (!g_shutdown_requested && restart_count < max_restarts) {
          restart_count++;
          spdlog::warn("Restarting application (attempt {}/{})", restart_count,
                       max_restarts);
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        } else {
          exit_code = 1;
          break;
        }
      }

      testServerConnection(app);
      try {
        owl::capnp::TestClient client("127.0.0.1:5346");
        client.testConnection();
      } catch (const std::exception &e) {
        spdlog::error("Failed to create client: {}", e.what());
        exit_code = 1;
        break;
      }

      spdlog::info(
          "Application initialized successfully. Starting main loop...");
      auto run_result = app.spin(g_shutdown_requested);

      if (run_result.is_ok()) {
        spdlog::info("Application finished successfully");
        exit_code = 0;
        break;
      } else {
        spdlog::error("Application finished with error: {}",
                      run_result.error().what());
        if (!g_shutdown_requested) {
          restart_count++;
          spdlog::warn("Restarting application (attempt {}/{})", restart_count,
                       max_restarts);
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        } else {
          exit_code = 1;
          break;
        }
      }

    } catch (const std::exception &e) {
      spdlog::critical("Unhandled exception in main: {}", e.what());
      if (!g_shutdown_requested && restart_count < max_restarts) {
        restart_count++;
        spdlog::warn("Restarting after exception (attempt {}/{})",
                     restart_count, max_restarts);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      } else {
        exit_code = 1;
        break;
      }
    }
  }

  if (restart_count >= max_restarts) {
    spdlog::critical("Maximum restart attempts reached. Exiting.");
    exit_code = 1;
  }

  stopFuseProcess(current_fuse_pid);
  spdlog::info("Application shutdown complete");
  return exit_code;
}