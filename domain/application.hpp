#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "network/capnproto.hpp"
#include <atomic>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>
#include <thread>

#include "algorithms/compressor/compressor_manager.hpp"
#include "embedded/emdedded_manager.hpp"
#include "file/fileinfo.hpp"
#include "ipc/ipc_pipeline_handler.hpp"
#include "markov.hpp"
#include "schemas/fileinfo.hpp"
#include "utils/quantization.hpp"
#include "vector_faiss_logic.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace owl::app {

template <typename EmbeddingModel = embedded::FastTextEmbedder,
          typename Compressor = compression::Compressor>
class Application {
public:
  Application(int argc, char **argv)
      : argc_(argc), argv_(argv),
        last_ranking_update_(std::chrono::steady_clock::now()),
        is_running_(false), server_address_("127.0.0.1:5346") {

    parseCommandLineArgs();
  }

  core::Result<bool> run(const std::string embedder_model,
                         bool use_quantization = false) {
    spdlog::info("Starting VectorFS Application initialization");

    std::vector<std::function<core::Result<bool>()>> init_steps = {
        [this]() { return initializeEventService(); },
        [this, &embedder_model]() {
          return initializeEmbedder(embedder_model);
        },
        [this, use_quantization]() {
          return initializeCompressor(use_quantization);
        },
        [this, use_quantization]() {
          return initializeQuantization(use_quantization);
        },
        [this]() { return initializeFileinfo(); },
        [this, use_quantization]() {
          return initializeFaiss(use_quantization);
        },
        [this]() { return initializeSemanticGraph(); },
        [this]() { return initializeHiddenMarkov(); },
        [this]() { return initializeIpc(); },
        [this]() { return initializePipeline(); },
        [this]() { return initializeServer(); }};

    auto init_result = initializeSequence(init_steps, 0);

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
    core::Result<bool>::Ok(true)
        .map([this, &address]() {
          std::string client_address =
              address.empty() ? server_address_ : address;
          auto client = std::make_unique<capnp::VectorFSClient>(client_address);
          spdlog::info("Client created for address: {}", client_address);
          return true;
        })
        .match([](bool) { /* success */ },
               [](const auto &error) {
                 spdlog::error("Failed to create client: {}", error.what());
               });
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
  core::Result<bool> initializeSequence(
      const std::vector<std::function<core::Result<bool>()>> &steps,
      size_t index) {
    if (index >= steps.size()) {
      return core::Result<bool>::Ok(true);
    }

    return steps[index]()
        .and_then([this, &steps, index]() -> core::Result<bool> {
          return initializeSequence(steps, index + 1);
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [this, index](const auto &error) -> core::Result<bool> {
              spdlog::error("Initialization step {} failed: {}", index,
                            error.what());
              return core::Result<bool>::Error(error.what());
            });
  }

  void parseCommandLineArgs() {
    core::Result<bool>::Ok(true)
        .map([this]() {
          for (int i = 1; i < argc_; ++i) {
            std::string arg = argv_[i];
            if (arg == "--server" && i + 1 < argc_) {
              server_address_ = argv_[++i];
              spdlog::info("Server address from command line: {}",
                           server_address_);
            } else if (arg == "--help") {
              printHelp();
            }
          }
          return true;
        })
        .match([](bool) { /* success */ },
               [](const auto &error) {
                 spdlog::error("Command line parsing failed: {}", error.what());
               });
  }

  void printHelp() {
    spdlog::info("VectorFS Application Usage:");
    spdlog::info(
        "  --server <address>    Set server address (default: 0.0.0.0:12345)");
    spdlog::info("  --help                Show this help message");
  }

  void cleanup() {
    core::Result<bool>::Ok(true)
        .map([this]() {
          spdlog::info("Cleaning up application resources");
          stopServer();

          if (ipc_base_) {
            ipc_base_->stop();
          }

          predictive_cache_.clear();
          spdlog::info("Cleanup completed");
          return true;
        })
        .match([](bool) { /* success */ },
               [](const auto &error) {
                 spdlog::error("Cleanup failed: {}", error.what());
               });
  }

  void updateRanking() {
    core::Result<bool>::Ok(true)
        .map([this]() {
          auto now = std::chrono::steady_clock::now();
          spdlog::debug("Updating ranking cache");
          last_ranking_update_ = now;
          return true;
        })
        .match([](bool) { /* success */ },
               [](const auto &error) {
                 spdlog::error("Ranking update exception: {}", error.what());
               });
  }

  void checkServerHealth() {
    getServerStatus().match(
        [](bool) { spdlog::debug("Server health check: OK"); },
        [](const auto &) { spdlog::warn("Server health check: NOT RUNNING"); });
  }

  core::Result<bool> initializeIpc() {
    return core::Result<bool>::Ok(true).and_then(
        [this]() -> core::Result<bool> {
          ipc_base_ = std::make_shared<IpcBaseService>("vectorfs_app",
                                                       "owl_ipc", "default");

          auto init_result = ipc_base_->initialize();
          if (!init_result.is_ok()) {
            return core::Result<bool>::Error("Failed to initialize IPC base");
          }

          auto publisher_result = ipc_base_->createPublisher();
          if (!publisher_result.is_ok()) {
            return core::Result<bool>::Error("Failed to create IPC publisher");
          }

          ipc_publisher_ = publisher_result.value();
          ipc_pipeline_handler_ = IpcPipelineHandler(ipc_publisher_);

          spdlog::info("IPC Pipeline Handler initialized successfully");
          return core::Result<bool>::Ok(true);
        });
  }

  core::Result<bool> initializeEventService() {
    return core::Result<bool>::Ok(true)
        .map([this]() {
          event_service_ = std::make_shared<core::Event>();
          event_service_->Subscribe<schemas::FileInfo>(
              [this](schemas::FileInfo &file) {
                this->create_file_pipeline_.process(file);
              });
          spdlog::info("Event service initialized");
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> initializeEmbedder(const std::string &model) {
    return embedder_.set(model)
        .map([this](bool) {
          spdlog::info("Embedder initialized successfully");
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> initializeCompressor(bool use_quantization) {
    return compressor_.set()
        .map([this](bool) {
          spdlog::info("Compressor initialized");
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> initializeQuantization(bool use_quantization) {
    return core::Result<bool>::Ok(true)
        .map([this, use_quantization]() {
          if (use_quantization) {
            sq_quantizer_ = std::make_unique<utils::ScalarQuantizer>();
            pq_quantizer_ = std::make_unique<utils::ProductQuantizer>();
            spdlog::info("Quantization initialized");
          }
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> initializeFileinfo() {
    return core::Result<bool>::Ok(true).map([]() {
      spdlog::info("Fileinfo initialized");
      return true;
    });
  }

  core::Result<bool> initializeFaiss(bool use_quantization) {
    return embedder_.embedder()
        .and_then(
            [this, use_quantization](auto &embedder) -> core::Result<bool> {
              return core::Result<bool>::Ok(true).map(
                  [this, &embedder, use_quantization]() {
                    int dimension = embedder.getDimension();
                    faiss_service_ = std::make_unique<faiss::FaissService>(
                        dimension, use_quantization);
                    spdlog::info("FAISS service initialized");
                    return true;
                  });
            })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> initializeSemanticGraph() {
    return core::Result<bool>::Ok(true).map([this]() {
      semantic_graph_ = markov::SemanticGraph();
      spdlog::info("Semantic graph initialized");
      return true;
    });
  }

  core::Result<bool> initializeHiddenMarkov() {
    return core::Result<bool>::Ok(true).map([this]() {
      hidden_markov_ = markov::HiddenMarkovModel();
      spdlog::info("Hidden Markov initialized");
      return true;
    });
  }

  core::Result<bool> initializePipeline() {
    return embedder_.embedder().and_then(
        [this](auto &embedder) -> core::Result<bool> {
          return compressor_.compressor().and_then(
              [this, &embedder](auto &compressor) -> core::Result<bool> {
                return core::Result<bool>::Ok(true).map(
                    [this, &embedder, &compressor]() {
                      create_file_pipeline_ = core::pipeline::Pipeline();
                      create_file_pipeline_.add_handler(embedder);
                      create_file_pipeline_.add_handler(compressor);
                      create_file_pipeline_.add_handler(ipc_pipeline_handler_);
                      spdlog::info("Pipeline with IPC initialized");
                      return true;
                    });
              });
        });
  }

  core::Result<bool> initializeServer() {
    return core::Result<bool>::Ok(true)
        .map([this]() {
          server_ = std::make_unique<capnp::VectorFSServer<EmbeddingModel>>(
              server_address_);
          server_->addEventService(event_service_);
          spdlog::info("Server initialized with address: {}", server_address_);
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              return core::Result<bool>::Error(error.what());
            });
  }

  core::Result<bool> startServer() {
    return core::Result<bool>::Ok(true).map([this]() {
      if (server_thread_ && server_->isRunning()) {
        spdlog::info("Server is already running");
        return true;
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
        return true;
      } else {
        throw std::runtime_error("Server failed to start");
      }
    });
  }

  core::Result<bool> stopServer() {
    return core::Result<bool>::Ok(true)
        .map([this]() {
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
          return true;
        })
        .match(
            [](bool success) -> core::Result<bool> {
              return core::Result<bool>::Ok(success);
            },
            [](const auto &error) -> core::Result<bool> {
              spdlog::warn("Server stop encountered issue: {}", error.what());
              return core::Result<bool>::Ok(true);
            });
  }

  void initializeHMMStates() {
    core::Result<bool>::Ok(true)
        .map([this]() {
          hidden_markov_.add_state("code");
          hidden_markov_.add_state("document");
          hidden_markov_.add_state("config");
          hidden_markov_.add_state("test");
          hidden_markov_.add_state("misc");
          spdlog::info("HMM states initialized");
          return true;
        })
        .match([](bool) { /* success */ },
               [](const auto &error) {
                 spdlog::error("HMM states initialization failed: {}",
                               error.what());
               });
  }

  int argc_;
  char **argv_;

  std::unique_ptr<faiss::FaissService> faiss_service_;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;

  std::shared_ptr<core::Event> event_service_;
  std::shared_ptr<IpcBaseService> ipc_base_;
  std::shared_ptr<IpcBaseService::PublisherType> ipc_publisher_;

  core::pipeline::Pipeline create_file_pipeline_;

  int fileinfo_create_subscribe_;

  std::atomic<bool> is_ipc_server_{false};

  EmbedderManager<EmbeddingModel> embedder_;
  CompressorManager<Compressor> compressor_;
  IpcPipelineHandler ipc_pipeline_handler_;

  markov::SemanticGraph semantic_graph_;
  markov::HiddenMarkovModel hidden_markov_;

  std::unique_ptr<capnp::VectorFSServer<EmbeddingModel>> server_;

  std::map<std::string, std::vector<std::string>> predictive_cache_;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update_;

  std::atomic<bool> is_running_;
  std::unique_ptr<std::thread> server_thread_;
  std::string server_address_;
};

} // namespace owl::app

#endif // OWL_APPLICATION