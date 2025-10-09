#ifndef OWL_STATE
#define OWL_STATE

#include "embedded/emdedded_manager.hpp"
#include "algorithms/compressor/compressor_manager.hpp"
#include "network/capnproto.hpp"
#include <memory>

namespace owl {

struct State {
  EmbedderManager<> embedder_;
  CompressorManager<> compressor_;
  std::unique_ptr<capnp::VectorFSServer<embedded::FastTextEmbedder>> server_;
};

} // namespace owl

#endif // OWL_STATE