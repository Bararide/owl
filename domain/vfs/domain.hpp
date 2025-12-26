#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <search.hpp>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "container_manager.hpp"
#include "env/cppenv.hpp"
#include "file/fileinfo.hpp"
#include <memory/container_builder.hpp>
#include <semantic/semantic_chunker.hpp>
#include <spdlog/spdlog.h>

namespace owl {

struct State {
  //   cppenv::EnvManager env_manager_;
  // chunkees::Search search_;
  // ContainerManager container_manager_;
  // EmbedderManager<> embedder_manager_;
  // semantic::SemanticChunker<>> text_chunker_;

  // std::shared_ptr<chunkees::Search> search_;
  // ContainerManager container_manager_;
  // EmbedderManager<> embedder_manager_;
  // semantic::SemanticChunker<> text_chunker_;
};

} // namespace owl

#endif // VECTORFS_STATE_HPP