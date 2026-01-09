#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <fuse3/fuse.h>

#include "vfs/core/container/container_manager.hpp"
#include "vfs/core/container/ossec_container.hpp"
#include "vfs/fs/processor/processor_base.hpp"

#include <infrastructure/event.hpp>
#include <libenvpp/env.hpp>
#include <semantic/semantic_chunker.hpp>
#include <spdlog/spdlog.h>

namespace owl {

constexpr auto kBaseContainerPath = "/home/bararide/.vectorfs/containers/";

constexpr auto kModelPath = "/home/bararide/code/models/crawl-300d-2M-subword/"
                            "crawl-300d-2M-subword.bin";

struct State {
  core::Event events_;

  using OssecContainerT = OssecContainer<EmbedderManager<>, chunkees::Search>;
  ContainerManager<OssecContainerT> container_manager_;

  EmbedderManager<> global_embedder_{kModelPath};
  chunkees::Search global_search_{global_embedder_};
  semantic::SemanticChunker<> text_chunker_{global_embedder_};
};

} // namespace owl

#endif // VECTORFS_STATE_HPP