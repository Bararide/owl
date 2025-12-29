#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <chrono>
#include <cstring>
#include <fuse3/fuse.h>
#include <map>
#include <memory>
#include <search.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

#include "container_manager.hpp"
#include "env/cppenv.hpp"
#include "file/fileinfo.hpp"
#include <memory/container_builder.hpp>
#include <semantic/semantic_chunker.hpp>
#include <spdlog/spdlog.h>

constexpr static auto kModelPath =
    "/home/bararide/code/models/crawl-300d-2M-subword/"
    "crawl-300d-2M-subword.bin";

namespace owl {

struct State {
  cppenv::EnvManager env_manager_;

  
  ContainerManager container_manager_;
  EmbedderManager<> embedder_manager_{kModelPath};

  chunkees::Search search_{embedder_manager_};
  semantic::SemanticChunker<> text_chunker_{embedder_manager_};

  // std::shared_ptr<chunkees::Search> search_;
  // ContainerManager container_manager_;
  // EmbedderManager<> embedder_manager_;
  // semantic::SemanticChunker<> text_chunker_;
};

} // namespace owl

#endif // VECTORFS_STATE_HPP