#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <memory>
#include <stdexcept>
#include <string>

#include "vectorfs.hpp"

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

  void test_semantic_search() noexcept { vector_fs_->test_semantic_search(); }
  void test_markov_model() noexcept {
    vector_fs_->generate_markov_test_result();
  }

  int initialize_fuse(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    return fuse_main(args.argc, args.argv, &(vector_fs_->get_operations()),
                     nullptr);
  }

  vectorfs::VectorFS &get_vector_fs() const { return *vector_fs_; }

  std::vector<float> get_embedding(const std::string &text) {
    return vector_fs_->get_embedding(text);
  }

  std::string get_embedder_info() const {
    return vector_fs_->get_embedder_info();
  }

private:
  VFSInstance(const std::string &model_path)
      : vector_fs_(vectorfs::VectorFS::getInstance()) {
    vector_fs_->initialize<EmbeddedModel, CompressorType>(model_path);
  }

  std::unique_ptr<vectorfs::VectorFS> vector_fs_;
  static std::unique_ptr<VFSInstance<EmbeddedModel, CompressorType>> instance_;
};

template <typename EmbeddedModel, typename CompressorType>
std::unique_ptr<VFSInstance<EmbeddedModel, CompressorType>>
    VFSInstance<EmbeddedModel, CompressorType>::instance_ = nullptr;

} // namespace owl::instance
#endif // INSTANCE_HPP