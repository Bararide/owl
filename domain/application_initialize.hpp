#ifndef OWL_APPLICATION_INITIALIZE
#define OWL_APPLICATION_INITIALIZE

#include "ipc/ipc_pipeline_handler.hpp"
#include "markov.hpp"
#include "network/capnproto.hpp"
#include "state.hpp"
#include "utils/constants.hpp"
#include "utils/quantization.hpp"
#include "vector_faiss_logic.hpp"
#include <functional>
#include <infrastructure/result.hpp>
#include <memory>
#include <vector>

namespace owl {

class InitializeManager {
public:
  static core::Result<bool> initializeAll(
      State &state, const std::string &embedder_model, bool use_quantization,
      std::shared_ptr<core::Event> &event_service,
      std::shared_ptr<IpcBaseService> &ipc_base,
      std::shared_ptr<IpcBaseService::PublisherType> &ipc_publisher,
      core::pipeline::Pipeline &create_file_pipeline,
      IpcPipelineHandler &ipc_pipeline_handler,
      markov::SemanticGraph &semantic_graph,
      markov::HiddenMarkovModel &hidden_markov,
      std::unique_ptr<faiss::FaissService> &faiss_service,
      std::unique_ptr<utils::ScalarQuantizer> &sq_quantizer,
      std::unique_ptr<utils::ProductQuantizer> &pq_quantizer,
      std::unique_ptr<capnp::VectorFSServer<embedded::FastTextEmbedder>>
          &server,
      const std::string &server_address) {

    std::vector<std::function<core::Result<bool>()>> init_steps = {
        [&]() { return initializeEventService(event_service); },
        [&]() { return initializeEmbedder(state, embedder_model); },
        [&]() { return initializeCompressor(state); },
        [&]() {
          return initializeQuantization(use_quantization, sq_quantizer,
                                        pq_quantizer);
        },
        [&]() { return initializeFileinfo(); },
        [&]() {
          return initializeFaiss(state, use_quantization, faiss_service);
        },
        [&]() { return initializeSemanticGraph(semantic_graph); },
        [&]() { return initializeHiddenMarkov(hidden_markov); },
        [&]() {
          return initializeIpc(ipc_base, ipc_publisher, ipc_pipeline_handler);
        },
        [&]() {
          return initializePipeline(state, event_service, ipc_pipeline_handler,
                                    create_file_pipeline);
        },
        [&]() {
          return initializeServer(event_service, server, server_address);
        }};

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

  static core::Result<bool>
  initializeEventService(std::shared_ptr<core::Event> &event_service) {
    event_service = std::make_shared<core::Event>();
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

  static core::Result<bool> initializeQuantization(
      bool use_quantization,
      std::unique_ptr<utils::ScalarQuantizer> &sq_quantizer,
      std::unique_ptr<utils::ProductQuantizer> &pq_quantizer) {
    if (use_quantization) {
      sq_quantizer = std::make_unique<utils::ScalarQuantizer>();
      pq_quantizer = std::make_unique<utils::ProductQuantizer>();
      spdlog::info("Quantization initialized");
    }
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool> initializeFileinfo() {
    spdlog::info("Fileinfo initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool>
  initializeFaiss(State &state, bool use_quantization,
                  std::unique_ptr<faiss::FaissService> &faiss_service) {
    return state.embedder_.embedder().and_then(
        [&](auto &embedder) -> core::Result<bool> {
          int dimension = embedder.getDimension();
          faiss_service = std::make_unique<faiss::FaissService>(
              dimension, use_quantization);
          spdlog::info("FAISS service initialized");
          return core::Result<bool>::Ok(true);
        });
  }

  static core::Result<bool>
  initializeSemanticGraph(markov::SemanticGraph &semantic_graph) {
    semantic_graph = markov::SemanticGraph();
    spdlog::info("Semantic graph initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool>
  initializeHiddenMarkov(markov::HiddenMarkovModel &hidden_markov) {
    hidden_markov = markov::HiddenMarkovModel();
    spdlog::info("Hidden Markov initialized");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool>
  initializeIpc(std::shared_ptr<IpcBaseService> &ipc_base,
                std::shared_ptr<IpcBaseService::PublisherType> &ipc_publisher,
                IpcPipelineHandler &ipc_pipeline_handler) {
    ipc_base =
        std::make_shared<IpcBaseService>("vectorfs_app", "owl_ipc", "default");

    auto init_result = ipc_base->initialize();
    if (!init_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize IPC base");
    }

    auto publisher_result = ipc_base->createPublisher();
    if (!publisher_result.is_ok()) {
      return core::Result<bool>::Error("Failed to create IPC publisher");
    }

    ipc_publisher = publisher_result.value();
    ipc_pipeline_handler = IpcPipelineHandler(ipc_publisher);

    spdlog::info("IPC Pipeline Handler initialized successfully");
    return core::Result<bool>::Ok(true);
  }

  static core::Result<bool>
  initializePipeline(State &state, std::shared_ptr<core::Event> &event_service,
                     IpcPipelineHandler &ipc_pipeline_handler,
                     core::pipeline::Pipeline &create_file_pipeline) {
    return state.embedder_.embedder().and_then(
        [&](auto &embedder) -> core::Result<bool> {
          return state.compressor_.compressor().and_then(
              [&](auto &compressor) -> core::Result<bool> {
                create_file_pipeline = core::pipeline::Pipeline();
                create_file_pipeline.add_handler(embedder);
                create_file_pipeline.add_handler(compressor);
                create_file_pipeline.add_handler(ipc_pipeline_handler);

                event_service->Subscribe<schemas::FileInfo>(
                    [&](schemas::FileInfo &file) {
                      create_file_pipeline.process(file);
                    });

                spdlog::info("Pipeline with IPC initialized");
                return core::Result<bool>::Ok(true);
              });
        });
  }

  static core::Result<bool> initializeServer(
      std::shared_ptr<core::Event> &event_service,
      std::unique_ptr<capnp::VectorFSServer<embedded::FastTextEmbedder>>
          &server,
      const std::string &server_address) {
    server =
        std::make_unique<capnp::VectorFSServer<embedded::FastTextEmbedder>>(
            server_address);
    server->addEventService(event_service);
    spdlog::info("Server initialized with address: {}", server_address);
    return core::Result<bool>::Ok(true);
  }
};

} // namespace owl

#endif // OWL_APPLICATION_INITIALIZE