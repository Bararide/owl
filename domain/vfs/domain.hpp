#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <fuse3/fuse.h>

#include "container_manager.hpp"
#include "env/cppenv.hpp"
#include <infrastructure/event.hpp>
#include <infrastructure/result.hpp>
#include <memory/container_builder.hpp>
#include <semantic/semantic_chunker.hpp>

constexpr static auto kModelPath =
    "/home/bararide/code/models/crawl-300d-2M-subword/"
    "crawl-300d-2M-subword.bin";

namespace owl {

struct State {
  cppenv::EnvManager env_manager_;

  core::Event events_;

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