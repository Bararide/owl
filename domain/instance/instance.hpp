#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <infrastructure/measure.hpp>
#include <memory>
#include <stdexcept>
#include <string>

#include "algorithms/compressor/compressor.hpp"
#include "state.hpp"
#include "vectorfs.hpp"
#include <search.hpp>

namespace owl::instance {

template <typename EmbeddedModel,
          typename CompressorType = owl::compression::Compressor>
class VFSInstance {
public:
  VFSInstance(const VFSInstance &) = delete;
  VFSInstance &operator=(const VFSInstance &) = delete;

  static VFSInstance<EmbeddedModel, CompressorType> &getInstance() {
    if (!instance_) {
      throw std::runtime_error(
          "VFSInstance not initialized. Call initialize() first.");
    }
    return *instance_;
  }

  static void initialize(const std::string &model_path) {
    if (instance_) {
      throw std::runtime_error("VFSInstance already initialized");
    }
    instance_ = std::unique_ptr<VFSInstance<EmbeddedModel, CompressorType>>(
        new VFSInstance<EmbeddedModel, CompressorType>(model_path));
  }

  static void shutdown() { instance_.reset(); }

  void test_semantic_search() noexcept {
    auto result = state_.search_.semanticSearch("test query", 5);
    if (result.is_ok()) {
      spdlog::info("Semantic search test completed successfully");
    } else {
      spdlog::warn("Semantic search test failed: {}", result.error().what());
    }
  }

  void test_markov_model() noexcept {
    auto result = state_.search_.enhancedSemanticSearchImpl("test", 3);
    spdlog::info("Markov model test completed");
  }

  int initialize_fuse(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    return fuse_main(args.argc, args.argv, &(vector_fs_->get_operations()),
                     nullptr);
  }

  vectorfs::VectorFS &get_vector_fs() const { return *vector_fs_; }
  chunkees::Search &get_search() { return state_.search_; }

  std::vector<float> get_embedding(const std::string &text) {
    auto embedder_result = state_.embedder_manager_.embedder();
    if (!embedder_result.is_ok()) {
      throw std::runtime_error("Embedder not initialized");
    }

    auto embedding_result = embedder_result.value().getSentenceEmbedding(text);
    if (!embedding_result.is_ok()) {
      throw std::runtime_error("Failed to get embedding");
    }

    return embedding_result.value();
  }

  std::string get_embedder_info() {
    auto embedder_result = state_.embedder_manager_.embedder();
    if (!embedder_result.is_ok()) {
      return "Embedder not initialized";
    }

    auto &embedder = embedder_result.value();
    return "Model: " + embedder.getModelName() +
           ", Dimension: " + std::to_string(embedder.getDimension());
  }

  vectorfs::State &get_state() { return state_; }

private:
  VFSInstance(const std::string &model_path)
      : embedder_manager_(),
        search_(embedder_manager_,
                {"code", "document", "config", "test", "misc"}),
        container_manager_(),
        state_(search_, container_manager_, embedder_manager_),
        vector_fs_(std::make_unique<vectorfs::VectorFS>(state_)) {

    container_manager_.set_search(search_);

    auto init_result = embedder_manager_.set(model_path);
    if (!init_result.is_ok()) {
      throw std::runtime_error(init_result.error().what());
    }

    spdlog::info("VFSInstance initialized with model: {}", model_path);
  }

  EmbedderManager<> embedder_manager_;
  chunkees::Search search_;
  vectorfs::ContainerManager container_manager_;
  vectorfs::State state_;
  std::unique_ptr<vectorfs::VectorFS> vector_fs_;

  static std::unique_ptr<VFSInstance<EmbeddedModel, CompressorType>> instance_;
};

template <typename EmbeddedModel, typename CompressorType>
std::unique_ptr<VFSInstance<EmbeddedModel, CompressorType>>
    VFSInstance<EmbeddedModel, CompressorType>::instance_ = nullptr;

} // namespace owl::instance
#endif // INSTANCE_HPP