#include "instance/instance.hpp"

int main(int argc, char *argv[]) {
  try {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting VectorFS...");

    const std::string fasttext_model_path =
        "/home/bararide/code/models/crawl-300d-2M-subword/"
        "crawl-300d-2M-subword.bin";

    vfs::instance::VFSInstance::initialize(fasttext_model_path);

    auto &vectorfs = vfs::instance::VFSInstance::getInstance();

    vectorfs.test_semantic_search();

    spdlog::info("Starting FUSE...");

    int result = vectorfs.initialize_fuse(argc, argv);

    vfs::instance::VFSInstance::shutdown();

    return result;

  } catch (const std::exception &e) {
    spdlog::error("Fatal error: {}", e.what());
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}