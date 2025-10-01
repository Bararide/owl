#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "network/capnproto.hpp"
#include <atomic>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>
#include <thread>

#include "algorithms/compressor/compressor.hpp"
#include "embedded/embedded_fasttext.hpp"
#include "file/fileinfo.hpp"
#include "markov.hpp"
#include "schemas/fileinfo.hpp"
#include "shared_memory/shared_memory.hpp"
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

using EmbedderVariant = std::variant<embedded::FastTextEmbedder>;
using CompressorVariant = std::variant<compression::Compressor>;

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
        [this]() { return initializeSharedMemory(); },
        [this]() { return initializePipeline(); },
        [this]() { return initializeServer(); }};

    for (size_t i = 0; i < init_steps.size(); ++i) {
      auto result = init_steps[i]();
      if (!result.is_ok()) {
        spdlog::error("Initialization step {} failed: {}", i,
                      result.error().what());
        cleanup();
        return core::Result<bool>::Error(result.error().what());
      }
    }

    initializeHMMStates();

    is_running_.store(true);
    spdlog::info("Application initialized successfully");

    return startServer();
  }

  core::Result<bool> spin(std::atomic<bool> &shutdown_requested) {
    spdlog::info("Starting main application loop");

    const auto health_check_interval = std::chrono::seconds(30);
    const auto ranking_update_interval = std::chrono::minutes(5);
    auto last_health_check = std::chrono::steady_clock::now();
    auto last_ranking_update = std::chrono::steady_clock::now();

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

        processEvents();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    } catch (const std::exception &e) {
      spdlog::error("Exception in main loop: {}", e.what());
      return core::Result<bool>::Error(e.what());
    }

    spdlog::info("Main application loop stopped");
    return core::Result<bool>::Ok(true);
  }

  core::Result<bool> shutdown() {
    spdlog::info("Initiating application shutdown");
    is_running_.store(false);
    stopServer();
    cleanup();
    return core::Result<bool>::Ok(true);
  }

  core::Result<bool> restartServer(const std::string &new_address = "") {
    if (!new_address.empty()) {
      server_address_ = new_address;
    }

    stopServer();
    return startServer();
  }

  core::Result<bool> getServerStatus() {
    if (!server_ || !server_->isRunning()) {
      return core::Result<bool>::Error("Server is not running");
    }
    return core::Result<bool>::Ok(true);
  }

  std::string getServerAddress() const { return server_address_; }

  void createClient(const std::string &address = "") {
    try {
      std::string client_address = address.empty() ? server_address_ : address;
      auto client = std::make_unique<capnp::VectorFSClient>(client_address);
      spdlog::info("Client created for address: {}", client_address);
    } catch (const std::exception &e) {
      spdlog::error("Failed to create client: {}", e.what());
    }
  }

  Application &operator=(const Application &) = delete;
  Application &operator=(Application &&) = delete;
  Application(const Application &) = delete;
  Application(Application &&) = delete;

  ~Application() { shutdown(); }

private:
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
        "  --server <address>    Set server address (default: 0.0.0.0:12345)");
    spdlog::info("  --help                Show this help message");
  }

  void cleanup() {
    spdlog::info("Cleaning up application resources");
    stopServer();
    predictive_cache_.clear();
    spdlog::info("Cleanup completed");
  }

  void processEvents() {
    try {
      // Обработка событий из pipeline
      // pipeline_.processPendingEvents();

      // Обработка событий из event_service
      // event_service_->processPendingEvents();

    } catch (const std::exception &e) {
      spdlog::error("Event processing exception: {}", e.what());
    }
  }

  void updateRanking() {
    try {
      auto now = std::chrono::steady_clock::now();
      spdlog::debug("Updating ranking cache");

      last_ranking_update_ = now;
    } catch (const std::exception &e) {
      spdlog::error("Ranking update exception: {}", e.what());
    }
  }

  void checkServerHealth() {
    if (server_ && server_->isRunning()) {
      spdlog::debug("Server health check: OK");
    } else {
      spdlog::warn("Server health check: NOT RUNNING");
    }
  }

  core::Result<bool> initializeEventService() {
    try {
      event_service_ = std::make_shared<core::Event>();

      event_service_->Subscribe<schemas::FileInfo>(
          [](const schemas::FileInfo &file) {
            spdlog::info("Received file info: {}", file.name.value());
          });

      spdlog::info("Event service initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeEmbedder(const std::string &model) {
    try {
      if constexpr (std::is_same_v<EmbeddingModel,
                                   embedded::FastTextEmbedder>) {
        embedder_ = embedded::FastTextEmbedder();
        std::get<embedded::FastTextEmbedder>(embedder_).loadModel(model);
      }

      spdlog::info(
          "Embedder initialized: {}",
          std::get<embedded::FastTextEmbedder>(embedder_).getEmbedderInfo());
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeCompressor(bool use_quantization) {
    try {
      if constexpr (std::is_same_v<Compressor, compression::Compressor>) {
        compressor_ = compression::Compressor();
      }
      spdlog::info("Compressor initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeQuantization(bool use_quantization) {
    try {
      if (use_quantization) {
        sq_quantizer_ = std::make_unique<utils::ScalarQuantizer>();
        pq_quantizer_ = std::make_unique<utils::ProductQuantizer>();
        spdlog::info("Quantization initialized");
      }
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeFileinfo() {
    try {
      spdlog::info("Fileinfo initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeFaiss(bool use_quantization) {
    try {
      int dimension = std::visit(
          [](const auto &embedder) { return embedder.getDimension(); },
          embedder_);

      faiss_service_ =
          std::make_unique<faiss::FaissService>(dimension, use_quantization);
      spdlog::info("FAISS service initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeSemanticGraph() {
    try {
      semantic_graph_ = markov::SemanticGraph();
      spdlog::info("Semantic graph initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeHiddenMarkov() {
    try {
      hidden_markov_ = markov::HiddenMarkovModel();
      spdlog::info("Hidden Markov initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeSharedMemory() {
    try {
      shm_manager_ = std::make_unique<shared::SharedMemoryManager>();
      if (shm_manager_->initialize()) {
        spdlog::info("Shared memory initialized");
        return core::Result<bool>::Ok(true);
      } else {
        return core::Result<bool>::Error("Failed to initialize shared memory");
      }
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializePipeline() {
    try {
      pipeline_ = core::pipeline::Pipeline();
      spdlog::info("Pipeline initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeServer() {
    try {
      server_ = std::make_unique<capnp::VectorFSServer<EmbeddingModel>>(
          server_address_);

      server_->addEventService(event_service_);

      spdlog::info("Server initialized with address: {}", server_address_);
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> startServer() {
    try {
      if (!server_) {
        return core::Result<bool>::Error("Server not initialized");
      }

      if (server_->isRunning()) {
        return core::Result<bool>::Error("Server is already running");
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

      if (server_->isRunning()) {
        spdlog::info("Server started successfully on address: {}",
                     server_address_);
        return core::Result<bool>::Ok(true);
      } else {
        return core::Result<bool>::Error("Server failed to start");
      }

    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  void stopServer() {
    if (server_) {
      server_->stop();
    }

    if (server_thread_ && server_thread_->joinable()) {
      spdlog::info("Stopping server thread...");
      server_thread_->join();
      server_thread_.reset();
      spdlog::info("Server thread stopped");
    }
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

  std::unique_ptr<faiss::FaissService> faiss_service_;
  std::unique_ptr<shared::SharedMemoryManager> shm_manager_;
  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;

  std::shared_ptr<core::Event> event_service_;

  core::pipeline::Pipeline pipeline_;

  int fileinfo_create_subscribe_;

  EmbedderVariant embedder_;
  CompressorVariant compressor_;
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