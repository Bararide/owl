#ifndef OWL_APPLICATION_INITIALIZE
#define OWL_APPLICATION_INITIALIZE

#include "state.hpp"
#include "utils/constants.hpp"
#include <functional>
#include <infrastructure/result.hpp>
#include <vector>

namespace owl {

class InitializeManager {
public:
  static core::Result<bool> initializeAll(State &state,
                                          const std::string &embedder_model,
                                          bool use_quantization,
                                          const std::string &server_address) {

    std::vector<std::function<core::Result<bool>()>> init_steps = {
        [&]() { return initializeEventService(state); },
        [&]() { return initializeEmbedder(state, embedder_model); },
        [&]() { return initializeCompressor(state); },
        [&]() { return initializeQuantization(state, use_quantization); },
        [&]() { return initializeFileinfo(); },
        [&]() { return initializeFaiss(state, use_quantization); },
        [&]() { return initializeSemanticGraph(state); },
        [&]() { return initializeHiddenMarkov(state); },
        [&]() { return initializeIpc(state); },
        [&]() { return initializePipeline(state); },
        [&]() { return initializeServer(state, server_address); }};

    return executeInitializationSequence(init_steps);
  }

private:
  static core::Result<bool> executeInitializationSequence(
      const std::vector<std::function<core::Result<bool>()>> &steps) {
    for (size_t i = 0; i < steps.size(); ++i) {
      auto result = steps[i]();
      if (!result.is_ok()) {
        spdlog::error("Initialization step {} failed: {}", i,
                      result.error().what());
        return result;
      }
    }
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeEventService(State &state) {
    state.event_service_ = std::make_shared<core::Event>();
    spdlog::info("Event service initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeEmbedder(State &state,
                                               const std::string &model) {
    return state.embedder_.set(model).map([](bool) {
      spdlog::info("Embedder initialized successfully");
      return true;
    });
  }

  static core::Result<bool> initializeCompressor(State &state) {
    return state.compressor_.set().map([](bool) {
      spdlog::info("Compressor initialized");
      return true;
    });
  }

  static core::Result<bool> initializeQuantization(State &state,
                                                   bool use_quantization) {
    if (use_quantization) {
      state.sq_quantizer_ = std::make_unique<utils::ScalarQuantizer>();
      state.pq_quantizer_ = std::make_unique<utils::ProductQuantizer>();
      spdlog::info("Quantization initialized");
    }
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeFileinfo() {
    spdlog::info("Fileinfo initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeFaiss(State &state,
                                            bool use_quantization) {
    return state.embedder_.embedder().and_then(
        [&](auto &embedder) -> core::Result<bool> {
          int dimension = embedder.getDimension();
          state.faiss_service_ = std::make_unique<faiss::FaissService>(
              dimension, use_quantization);
          spdlog::info("FAISS service initialized");
          return core::Result<bool>::Ok(true);
        });
  }

  static core::Result<bool> initializeSemanticGraph(State &state) {
    state.semantic_graph_ = markov::SemanticGraph();
    spdlog::info("Semantic graph initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeHiddenMarkov(State &state) {
    state.hidden_markov_ = markov::HiddenMarkovModel();
    spdlog::info("Hidden Markov initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeIpc(State &state) {
    state.ipc_base_ =
        std::make_shared<IpcBaseService>("vectorfs_app", "owl_ipc", "default");

    auto init_result = state.ipc_base_->initialize();
    if (!init_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize IPC base");
    }

    auto publisher_result = state.ipc_base_->createPublisher();
    if (!publisher_result.is_ok()) {
      return core::Result<bool>::Error("Failed to create IPC publisher");
    }

    state.ipc_publisher_ = publisher_result.value();
    state.ipc_pipeline_handler_ = IpcPipelineHandler(state.ipc_publisher_);

    spdlog::info("IPC Pipeline Handler initialized successfully");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializePipeline(State &state) {
    return state.embedder_.embedder().and_then(
        [&](auto &embedder) -> core::Result<bool> {
          return state.compressor_.compressor().and_then(
              [&](auto &compressor) -> core::Result<bool> {
                state.create_file_pipeline_ = core::pipeline::Pipeline();
                state.create_file_pipeline_.add_handler(embedder);
                state.create_file_pipeline_.add_handler(compressor);
                state.create_file_pipeline_.add_handler(
                    state.ipc_pipeline_handler_);

                state.event_service_->Subscribe<schemas::FileInfo>(
                    [&](schemas::FileInfo &file) {
                      state.create_file_pipeline_.process(file);
                    });

                spdlog::info("Pipeline with IPC initialized");
                return core::Result<bool>::Ok(true);
              });
        });
  }

  static core::Result<bool>
  initializeServer(State &state, const std::string &server_address) {
    state.server_ =
        std::make_unique<capnp::VectorFSServer<embedded::FastTextEmbedder>>(
            server_address);
    state.server_->addEventService(state.event_service_);
    spdlog::info("Server initialized with address: {}", server_address);
    return core::Result<bool>::Ok(true);
  }
};

} // namespace owl

#endif // OWL_APPLICATION_INITIALIZE