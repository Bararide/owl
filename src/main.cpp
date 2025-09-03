#include "instance/instance.hpp"
#include "network/network.hpp"

int main(int argc, char *argv[]) {
  try {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting VectorFS...");

    const std::string fasttext_model_path =
        "/home/bararide/code/models/crawl-300d-2M-subword/"
        "crawl-300d-2M-subword.bin";

    auto start = std::chrono::high_resolution_clock::now();
    vfs::instance::VFSInstance::initialize(fasttext_model_path);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("VectorFS initialized in {} ms", duration);

    start = std::chrono::high_resolution_clock::now();
    auto &vectorfs = vfs::instance::VFSInstance::getInstance();
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("VectorFS loaded in {} ms", duration);

    start = std::chrono::high_resolution_clock::now();
    vectorfs.test_semantic_search();
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    spdlog::info("Semantic search test completed in {} ms", duration);

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