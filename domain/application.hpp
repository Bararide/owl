#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "network/capnproto.hpp"
#include <atomic>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>

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
        is_running_(false) {}

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
    return core::Result<bool>::Ok(true);
  }

  core::Result<bool> spin(std::atomic<bool> &shutdown_requested) {
    spdlog::info("Starting main application loop");

    const auto health_check_interval = std::chrono::seconds(30);
    const auto ranking_update_interval = std::chrono::minutes(5);
    auto last_health_check = std::chrono::steady_clock::now();
    auto last_ranking_update = std::chrono::steady_clock::now();

    try {
      while (!shutdown_requested && is_running_.load()) {

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
    cleanup();
    return core::Result<bool>::Ok(true);
  }

  Application &operator=(const Application &) = delete;
  Application &operator=(Application &&) = delete;
  Application(const Application &) = delete;
  Application(Application &&) = delete;

  ~Application() { shutdown(); }

private:
  void cleanup() {
    spdlog::info("Cleaning up application resources");

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
      server_ = capnp::VectorFSServiceImpl<EmbeddingModel>();

      auto result = server_.addEventService(event_service_);
      if (!result.is_ok()) {
        return core::Result<bool>::Error(result.error());
      }

      // Запуск сервера в отдельном потоке, если нужно
      // server_.startAsync();

      spdlog::info("Server initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
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
  capnp::VectorFSServiceImpl<EmbeddingModel> server_;

  std::map<std::string, std::vector<std::string>> predictive_cache_;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update_;

  std::atomic<bool> is_running_;
};

} // namespace owl::app

#endif // OWL_APPLICATION