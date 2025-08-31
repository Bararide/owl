#include "vectorfs.hpp"

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    
    spdlog::set_level(spdlog::level::info);

    auto& vectorfs = *vectorfs::VectorFS::getInstance();
    
    const char* fasttext_model_path = "/home/bararide/code/models/crawl-300d-2M-subword/crawl-300d-2M-subword.bin";
    
    if (!vectorfs.initialize(fasttext_model_path)) {
        return 1;
    }

    vectorfs.test_semantic_search();

    auto& ops = vectorfs::VectorFS::get_operations();
    return fuse_main(args.argc, args.argv, &ops, nullptr);
}