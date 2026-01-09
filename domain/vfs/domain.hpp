#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <fuse3/fuse.h>

#include "container_manager.hpp"
#include "vfs/fs/processor/processor_base.hpp"
#include <infrastructure/event.hpp>
#include <infrastructure/result.hpp>
#include <libenvpp/env.hpp>
#include <memory/container_builder.hpp>
#include <semantic/semantic_chunker.hpp>

namespace owl {

auto pre = env::prefix("OWL");

const auto kBaseContainerPath =
    pre.register_variable<std::string_view>("BASE_CONTAINERS_PATH");

const auto kModelPath =
    pre.register_variable<std::string>("FASTEXT_MODEL_PATH");

const auto kParsedAndValidatedPre = pre.parse_and_validate();

struct State {
  core::Event events_;

  ContainerManager container_manager_;
  EmbedderManager<> embedder_manager_{kParsedAndValidatedPre.get(kModelPath).value()};

  chunkees::Search search_{embedder_manager_};
  semantic::SemanticChunker<> text_chunker_{embedder_manager_};

  // std::shared_ptr<chunkees::Search> search_;
  // ContainerManager container_manager_;
  // EmbedderManager<> embedder_manager_;
  // semantic::SemanticChunker<> text_chunker_;
};

} // namespace owl

#endif // VECTORFS_STATE_HPP