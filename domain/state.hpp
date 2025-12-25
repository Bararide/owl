// #ifndef VECTORFS_STATE_HPP
// #define VECTORFS_STATE_HPP

// #include <chrono>
// #include <cstring>
// #include <map>
// #include <memory>
// #include <search.hpp>
// #include <set>
// #include <string>
// #include <unistd.h>
// #include <vector>

// #include "container_manager.hpp"
// #include "env/cppenv.hpp"
// #include "file/fileinfo.hpp"
// #include <memory/container_builder.hpp>
// #include <semantic/semantic_chunker.hpp>
// #include <spdlog/spdlog.h>

// namespace owl {

// struct State {
//   cppenv::EnvManager env_manager_;
//   chunkees::Search search_;
//   // ContainerManager container_manager_;
//   // EmbedderManager<> embedder_manager_;
//   // semantic::SemanticChunker<>> text_chunker_;

//   // std::shared_ptr<chunkees::Search> search_;
//   ContainerManager container_manager_;
//   EmbedderManager<> embedder_manager_;
//   semantic::SemanticChunker<> text_chunker_;
// };

// } // namespace owl

// #endif // VECTORFS_STATE_HPP

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
public:
  State(std::shared_ptr<chunkees::Search> search,
        std::shared_ptr<ContainerManager> container_manager,
        std::shared_ptr<EmbedderManager<>> embedder_manager)
      : search_(std::move(search)),
        container_manager_(std::move(container_manager)),
        embedder_manager_(std::move(embedder_manager)) {
    
    if (!env_manager_.load_from_file("../.env")) {
      spdlog::error("Error load env file");
    }
    
    if (embedder_manager_) {
      text_chunker_ = std::make_shared<semantic::SemanticChunker<>>(*embedder_manager_);
    } else {
      spdlog::error("EmbedderManager not available for SemanticChunker initialization");
    }
  }

  cppenv::EnvManager env_manager_;
  std::shared_ptr<chunkees::Search> search_;
  std::shared_ptr<ContainerManager> container_manager_;
  std::shared_ptr<EmbedderManager<>> embedder_manager_;
  std::shared_ptr<semantic::SemanticChunker<>> text_chunker_;
  

  chunkees::Search &getSearch() {
    if (!search_)
      throw std::runtime_error("Search not initialized");
    return *search_;
  }

  ContainerManager &getContainerManager() {
    if (!container_manager_) {
      throw std::runtime_error("ContainerManager not initialized");
    }
    return *container_manager_;
  }

  EmbedderManager<> &getEmbedderManager() {
    if (!embedder_manager_) {
      throw std::runtime_error("EmbedderManager<> not initialized");
    }
    return *embedder_manager_;
  }

  semantic::SemanticChunker<> &getSemanticChunker() {
    if (!text_chunker_) {
      throw std::runtime_error("SemanticChunker<> not initialized");
    }
    return *text_chunker_;
  }
};

} // namespace owl

#endif // VECTORFS_STATE_HPP