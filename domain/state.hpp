#ifndef OWL_STATE
#define OWL_STATE

#include "algorithms/compressor/compressor_manager.hpp"
#include "embedded/emdedded_manager.hpp"
#include "ipc/ipc_pipeline_handler.hpp"
#include "markov.hpp"
#include "network/capnproto.hpp"
#include "utils/quantization.hpp"
#include "vector_faiss_logic.hpp"
#include <memory>

namespace owl {

struct State {
  EmbedderManager<> embedder_;
  CompressorManager<> compressor_;

  std::unique_ptr<capnp::VectorFSServer<embedded::FastTextEmbedder>> server_;

  std::shared_ptr<core::Event> event_service_;
  std::shared_ptr<IpcBaseService> ipc_base_;
  std::shared_ptr<IpcBaseService::PublisherType> ipc_publisher_;
  IpcPipelineHandler ipc_pipeline_handler_;

  core::pipeline::Pipeline create_file_pipeline_;

  std::unique_ptr<faiss::FaissService> faiss_service_;
  markov::SemanticGraph semantic_graph_;
  markov::HiddenMarkovModel hidden_markov_;

  std::unique_ptr<utils::ScalarQuantizer> sq_quantizer_;
  std::unique_ptr<utils::ProductQuantizer> pq_quantizer_;

  std::map<std::string, std::vector<std::string>> predictive_cache_;
  std::chrono::time_point<std::chrono::steady_clock> last_ranking_update_;

  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_ipc_server_{false};
};

} // namespace owl

#endif // OWL_STATE