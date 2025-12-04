#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <infrastructure/measure.hpp>
#include <memory>
#include <stdexcept>
#include <string>

#include "state.hpp"
#include "vectorfs.hpp"
#include <search.hpp>

namespace owl::instance {

template <typename EmbeddedModel>
class VFSInstance {
public:
  VFSInstance(const VFSInstance &) = delete;
  VFSInstance &operator=(const VFSInstance &) = delete;

  static VFSInstance<EmbeddedModel> &getInstance() {
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
    instance_ = std::unique_ptr<VFSInstance<EmbeddedModel>>(
        new VFSInstance<EmbeddedModel>(model_path));
  }

  static void shutdown() {
    if (instance_) {
      instance_->cleanup();
      instance_.reset();
    }
  }

  void test_semantic_search() noexcept {
    if (vector_fs_) {
      vector_fs_->test_semantic_search();
    }
  }

  void test_markov_model() noexcept {
    if (vector_fs_) {
      vector_fs_->test_markov_chains();
    }
  }

  void test_container() noexcept {
    if (vector_fs_) {
      vector_fs_->test_container();
    }
  }

  int initialize_fuse(int argc, char *argv[]) {
    if (!vector_fs_) {
      throw std::runtime_error("VectorFS not initialized");
    }
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    return fuse_main(args.argc, args.argv, &(vector_fs_->get_operations()),
                     nullptr);
  }

  std::vector<float> get_embedding(const std::string &text) {
    if (!embedder_manager_) {
      throw std::runtime_error("EmbedderManager<> not initialized");
    }

    auto embedder_result = embedder_manager_->embedder();
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
    if (!embedder_manager_) {
      return "EmbedderManager not initialized";
    }

    auto embedder_result = embedder_manager_->embedder();
    if (!embedder_result.is_ok()) {
      return "Embedder not initialized";
    }

    auto &embedder = embedder_result.value();
    return "Model: " + embedder.getModelName() +
           ", Dimension: " + std::to_string(embedder.getDimension());
  }

  vectorfs::VectorFS &get_vector_fs() const { return *vector_fs_; }
  chunkees::Search &getSearch() {
    if (!search_) {
      throw std::runtime_error("Search not initialized");
    }
    return *search_;
  }

  vectorfs::State &get_state() {
    if (!state_) {
      throw std::runtime_error("State not initialized");
    }
    return *state_;
  }

private:
  VFSInstance(const std::string &model_path) {
    try {
      embedder_manager_ = std::make_shared<EmbedderManager<>>();

      auto init_result = embedder_manager_->set(model_path);
      if (!init_result.is_ok()) {
        throw std::runtime_error(init_result.error().what());
      }

      container_manager_ = std::make_shared<vectorfs::ContainerManager>();
      search_ = std::make_shared<chunkees::Search>(
          *embedder_manager_,
          std::vector<std::string>{"code", "document", "config", "test",
                                   "misc"});

      state_ = std::make_unique<vectorfs::State>(search_, container_manager_,
                                                 embedder_manager_);

      vector_fs_ = std::make_unique<vectorfs::VectorFS>(*state_);

      container_manager_->set_embedder(*embedder_manager_);

      spdlog::info("VFSInstance initialized with model: {}", model_path);
    } catch (const std::exception &e) {
      cleanup();
      throw;
    }
  }

  void cleanup() {
    vector_fs_.reset();
    state_.reset();
    search_.reset();
    container_manager_.reset();
    embedder_manager_.reset();
  }

  std::shared_ptr<EmbedderManager<>> embedder_manager_;
  std::shared_ptr<chunkees::Search> search_;
  std::shared_ptr<vectorfs::ContainerManager> container_manager_;
  std::unique_ptr<vectorfs::State> state_;
  std::unique_ptr<vectorfs::VectorFS> vector_fs_;

  static std::unique_ptr<VFSInstance<EmbeddedModel>> instance_;
};

template <typename EmbeddedModel>
std::unique_ptr<VFSInstance<EmbeddedModel>>
    VFSInstance<EmbeddedModel>::instance_ = nullptr;

} // namespace owl::instance
#endif // INSTANCE_HPP