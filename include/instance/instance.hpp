#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include "vectorfs.hpp"
#include <memory>
#include <string>
#include <utility>

namespace vfs::instance {

class VFSInstance {
public:
  VFSInstance(const VFSInstance &) = delete;
  VFSInstance &operator=(const VFSInstance &) = delete;

  static VFSInstance &getInstance() {
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
    instance_ = std::unique_ptr<VFSInstance>(new VFSInstance(model_path));
  }

  static void shutdown() { instance_.reset(); }

  void test_semantic_search() noexcept { vector_fs_->test_semantic_search(); }

  int initialize_fuse(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    return fuse_main(args.argc, args.argv, &(vector_fs_->get_operations()),
                     nullptr);
  }

  vectorfs::VectorFS &get_vector_fs() const { return *vector_fs_; }

private:
  VFSInstance(const std::string &model_path)
      : vector_fs_(vectorfs::VectorFS::getInstance()) {
    vector_fs_->initialize(model_path);
  }

  std::unique_ptr<vectorfs::VectorFS> vector_fs_;
  static std::unique_ptr<VFSInstance> instance_;
};

std::unique_ptr<VFSInstance> VFSInstance::instance_ = nullptr;

} // namespace vfs::instance

#endif // INSTANCE_HPP