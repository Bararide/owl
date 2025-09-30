#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "network/capnproto.hpp"
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
        last_ranking_update_(std::chrono::steady_clock::now()) {
    event_service_->Subscribe<schemas::FileInfo>(
        [](const schemas::FileInfo &file) {

        });
  }

  core::Result<bool> run(const std::string embedder_model,
                         bool use_quantization = false) {
    spdlog::info("Starting VectorFS Application");

    auto initialize_event_service = initializeEventService();
    if (!initialize_event_service.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize event service");
    }

    auto initialize_embedder = initializeEmbrdder(embedder_model);
    if (!initialize_embedder.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize embedder");
    }

    auto initialize_compressor = initializeCompressor(use_quantization);
    if (!initialize_compressor.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize compressor");
    }

    auto initialize_fileinfo = initializeFileinfo();
    if (!initialize_fileinfo.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize fileinfo");
    }

    auto initialize_quantization = initializeQuantization(use_quantization);

    if (!initialize_quantization.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize quantization");
    }

    auto initialize_faiss = initializeFaiss(use_quantization);
    if (!initialize_faiss.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize FAISS service");
    }

    auto initialize_semantic_graph = initializeSemanticGraph();
    if (!initialize_semantic_graph.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize semantic graph");
    }

    auto initialize_hidden_markov = initializeHiddenMarkov();
    if (!initialize_hidden_markov.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize hidden markov");
    }

    auto initialize_shm = initializeSharedMemory();
    if (!initialize_shm.is_ok()) {
      spdlog::warn("Failed to initialize shared memory");
    }

    auto pipeline_result = initializePipeline();
    if (!pipeline_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize pipeline");
    }

    auto server_result = initializeServer();
    if (!server_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize server");
    }

    initializeHMMStates();

    spdlog::info("Application initialized successfully");
    return core::Result<bool>::Ok(true);
  }

  Application &operator=(const Application &) = delete;
  Application &operator=(Application &&) = delete;
  Application(const Application &) = delete;
  Application(Application &&) = delete;

private:
  core::Result<bool> initializeEventService() {
    try {
      event_service_ = std::make_shared<core::Event>();
      spdlog::info("Event service initialized");
      return core::Result<bool>::Ok(true);
    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeEmbrdder(const std::string model) {
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
};

} // namespace owl::app

#endif // OWL_APPLICATION