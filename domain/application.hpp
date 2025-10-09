#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "application_initialize.hpp"
#include "ipc/ipc_pipeline_handler.hpp"
#include "markov.hpp"
#include "network/capnproto.hpp"
#include "state.hpp"
#include "utils/quantization.hpp"
#include "vector_faiss_logic.hpp"
#include <atomic>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <thread>

namespace owl::app {

class Application {
public:
  Application(int argc, char **argv)
      : argc_(argc), argv_(argv), state_(),
        last_ranking_update_(std::chrono::steady_clock::now()),
        is_running_(false), server_address_("127.0.0.1:5346") {
    parseCommandLineArgs();
  }

  core::Result<bool> run(const std::string &embedder_model,
                         bool use_quantization = false) {
    spdlog::info("Starting VectorFS Application initialization");

    auto init_result = initializeAll(embedder_model, use_quantization);

    return init_result
        .and_then([this]() -> core::Result<bool> {
          initializeHMMStates();
          is_running_.store(true);
          spdlog::info("Application initialized successfully");
          return startServer();
        })
        .match(
            [](bool) -> core::Result<bool> {
              return core::Result<bool>::Ok(true);
            },
            [this](const auto &error) -> core::Result<bool> {
              cleanup();
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> spin(std::atomic<bool> &shutdown_requested) {
    spdlog::info("Starting main application loop");

    const auto health_check_interval = std::chrono::seconds(30);
    const auto ranking_update_interval = std::chrono::minutes(5);
    auto last_health_check = std::chrono::steady_clock::now();
    auto last_ranking_update = std::chrono::steady_clock::now();

    return core::Result<bool>::Ok(true).and_then([&]() -> core::Result<bool> {
      try {
        while (!shutdown_requested && is_running_.load()) {
          auto now = std::chrono::steady_clock::now();

          if (now - last_health_check >= health_check_interval) {
            checkServerHealth();
            last_health_check = now;
          }

          if (now - last_ranking_update >= ranking_update_interval) {
            updateRanking();
            last_ranking_update = now;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("Main application loop stopped");
        return core::Result<bool>::Ok(true);
      } catch (const std::exception &e) {
        spdlog::error("Exception in main loop: {}", e.what());
        return core::Result<bool>::Error(e.what());
      }
    });
  }

  core::Result<bool> shutdown() {
    spdlog::info("Initiating application shutdown");
    is_running_.store(false);

    return stopServer().match(
        [this](bool) -> core::Result<bool> {
          cleanup();
          return core::Result<bool>::Ok(true);
        },
        [this](const auto &error) -> core::Result<bool> {
          cleanup();
          return core::Result<bool>::Ok(true);
        });
  }

  core::Result<bool> restartServer(const std::string &new_address = "") {
    return stopServer().and_then([this, &new_address]() -> core::Result<bool> {
      if (!new_address.empty()) {
        server_address_ = new_address;
      }
      return startServer();
    });
  }

  core::Result<bool> getServerStatus() {
    return core::Result<bool>::Ok(server_ && server_->isRunning())
        .and_then([](bool is_running) -> core::Result<bool> {
          return is_running
                     ? core::Result<bool>::Ok(true)
                     : core::Result<bool>::Error("Server is not running");
        });
  }

  std::string getServerAddress() const { return server_address_; }

  void createClient(const std::string &address = "") {
    std::string client_address = address.empty() ? server_address_ : address;
    auto client = std::make_unique<capnp::VectorFSClient>(client_address);
    spdlog::info("Client created for address: {}", client_address);
  }

  Application &operator=(const Application &) = delete;
  Application &operator=(Application &&) = delete;
  Application(const Application &) = delete;
  Application(Application &&) = delete;

  ~Application() {
    if (is_running_.load()) {
      shutdown();
    }

    if (server_thread_ && server_thread_->joinable()) {
      server_thread_->detach();
    }
  }

private:
  core::Result<bool> initializeAll(const std::string &embedder_model,
                                   bool use_quantization) {
    return InitializeManager::initializeAll(
        state_, embedder_model, use_quantization, event_service_, ipc_base_,
        ipc_publisher_, create_file_pipeline_, ipc_pipeline_handler_,
        semantic_graph_, hidden_markov_, faiss_service_, sq_quantizer_,
        pq_quantizer_, server_, server_address_);
  }

  void parseCommandLineArgs() {
    for (int i = 1; i < argc_; ++i) {
      std::string arg = argv_[i];
      if (arg == "--server" && i + 1 < argc_) {
        server_address_ = argv_[++i];
        spdlog::info("Server address from command line: {}", server_address_);
      } else if (arg == "--help") {
        printHelp();
      }
    }
  }

  void printHelp() {
    spdlog::info("VectorFS Application Usage:");
    spdlog::info(
        "  --server <address>    Set server address (default: 127.0.0.1:5346)");
    spdlog::info("  --help                Show this help message");
  }

  void cleanup() {
    spdlog::info("Cleaning up application resources");
    stopServer();

    if (ipc_base_) {
      ipc_base_->stop();
    }

    predictive_cache_.clear();
    spdlog::info("Cleanup completed");
  }

  void updateRanking() {
    auto now = std::chrono::steady_clock::now();
    spdlog::debug("Updating ranking cache");
    last_ranking_update_ = now;
    spdlog::info("update ranking success");
  }

  void checkServerHealth() {
    getServerStatus().match(
        [](bool) { spdlog::debug("Server health check: OK"); },
        [](const auto &) { spdlog::warn("Server health check: NOT RUNNING"); });
  }

  core::Result<bool> startServer() {
    if (server_thread_ && server_->isRunning()) {
      spdlog::info("Server is already running");
      return core::Result<bool>::Ok(true);
    }

    if (server_thread_ && server_thread_->joinable()) {
      server_thread_->join();
      server_thread_.reset();
    }

    server_thread_ = std::make_unique<std::thread>([this]() {
      spdlog::info("Starting server on thread for address: {}",
                   server_address_);
      try {
        server_->run();
      } catch (const std::exception &e) {
        spdlog::error("Server thread exception: {}", e.what());
      }
      spdlog::info("Server thread stopped");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (server_->isRunning()) {
      spdlog::info("Server started successfully on address: {}",
                   server_address_);
      return core::Result<bool>::Ok(true);
    } else {
      return core::Result<bool>::Error("Server failed to start");
    }
  }

  core::Result<bool> stopServer() {
    if (server_) {
      spdlog::info("Stopping server...");
      server_->stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (server_thread_ && server_thread_->joinable()) {
      spdlog::info("Stopping server thread...");
      server_thread_->join();
      server_thread_.reset();
      spdlog::info("Server thread stopped");
    }

    return core::Result<bool>::Ok(true);
  }

  void initializeHMMStates() {
    hidden_markov_.add_state("code");
    hidden_markov_.add_state("document");
    hidden_markov_.add_state("config");
    hidden_markov_.add_state("test");
    hidden_markov_.add_state("misc");
    spdlog::info("HMM states initialized");
  }

  int argc_;
  char **argv_;

  State state_;

  std::unique_ptr<faiss::FaissService> faiss_service_;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;

  std::shared_ptr<core::Event> event_service_;
  std::shared_ptr<IpcBaseService> ipc_base_;
  std::shared_ptr<IpcBaseService::PublisherType> ipc_publisher_;

  core::pipeline::Pipeline create_file_pipeline_;
  IpcPipelineHandler ipc_pipeline_handler_;

  markov::SemanticGraph semantic_graph_;
  markov::HiddenMarkovModel hidden_markov_;

  std::unique_ptr<capnp::VectorFSServer<embedded::FastTextEmbedder>> server_;

  std::map<std::string, std::vector<std::string>> predictive_cache_;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update_;

  std::atomic<bool> is_running_;
  std::unique_ptr<std::thread> server_thread_;
  std::string server_address_;
};

} // namespace owl::app

#endif // OWL_APPLICATION