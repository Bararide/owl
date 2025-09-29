#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "network/capnproto.hpp"
#include <memory>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>

#include "algorithms/compressor/compressor.hpp"
#include "embedded/embedded_fasttext.hpp"

namespace owl::app {

using EmbedderVariant = std::variant<embedded::FastTextEmbedder>;

using CompressorVariant = std::variant<compression::Compressor>;

template <typename EmbeddingModel = embedded::FastTextEmbedder,
          typename Compressor = compression::Compressor>
class Application {
public:
  Application(int argc, char **argv) : argc_(argc), argv_(argv) {}

  core::Result<bool> run(const std::string embedder_model,
                         bool use_quantization = false) {
    spdlog::info("Starting VectorFS Application");

    auto initialize_embedder = initializeEmbrdder(embedder_model);

    if (!initialize_embedder.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize embedder");
    }

    auto pipeline_result = initializePipeline();
    if (!pipeline_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize pipeline");
    }

    auto server_result = initializeServer();
    if (!server_result.is_ok()) {
      return core::Result<bool>::Error("Failed to initialize server");
    }

    spdlog::info("Application initialized successfully");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // capnp_server_->stop();

    return core::Result<bool>::Ok(true);
  }

  Application &operator=(const Application &) = delete;
  Application &operator=(Application &&) = delete;
  Application(const Application &) = delete;
  Application(Application &&) = delete;

private:
  core::Result<bool> initializeEmbrdder(const std::string model) {
    try {
      if constexpr (std::is_same_v<EmbeddingModel,
                                   embedded::FastTextEmbedder>) {
        embedder_ = embedded::FastTextEmbedder();
        std::get<embedded::FastTextEmbedder>(embedder_).loadModel(model);
      }

      spdlog::info("Embedder initialized");

      return core::Result<bool>::Ok(true);

    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializePipeline() {
    try {
      pipeline_ = core::pipeline::Pipeline();

      //   auto markov_handler =
      //       std::make_shared<handlers::MarkovHandler<MarkovModel>>();
      //   auto embedding_handler =
      //       std::make_shared<handlers::EmbeddingHandler<EmbeddingModel>>();
      //   auto compression_handler =
      //       std::make_shared<handlers::CompressionHandler>();
      //   auto file_create_handler =
      //       std::make_shared<handlers::FileCreateHandler>();

      //   pipeline_->add_handler(markov_handler);
      //   pipeline_->add_handler(embedding_handler);
      //   pipeline_->add_handler(compression_handler);
      //   pipeline_->add_handler(file_create_handler);

      spdlog::info("Pipeline initialized with {} handlers", pipeline_.size());
      spdlog::info("Pipeline structure:\n{}", pipeline_.describe());

      return core::Result<bool>::Ok(true);

    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  core::Result<bool> initializeServer() {
    try {
      //   capnp_server_ =
      //       std::make_shared<owl::capnp::VectorFSServiceImpl<MarkovModel>>(
      //           pipeline_);
      //   auto result = capnp_server_->start();

      //   if (!result.is_ok()) {
      //     return core::Result<bool>::Error(result.error().what());
      //   }

      //   spdlog::info("Cap'n Proto server initialized");
      return core::Result<bool>::Ok(true);

    } catch (const std::exception &e) {
      return core::Result<bool>::Error(e.what());
    }
  }

  int argc_;
  char **argv_;
  core::pipeline::Pipeline pipeline_;
  //   std::shared_ptr<owl::capnp::VectorFSServiceImpl<MarkovModel>>
  //   capnp_server_;

  EmbedderVariant embedder_;
  CompressorVariant compressor_;
};

} // namespace owl::app

#endif // OWL_APPLICATION