#ifndef OWL_EMBEDDERD_MANAGER
#define OWL_EMBEDDERD_MANAGER

#include "embedded_fasttext.hpp"

namespace owl {

using namespace embedded;

using EmbedderVariant = std::variant<embedded::FastTextEmbedder>;

template <typename T = FastTextEmbedder> class EmbedderManager {
public:
  EmbedderManager() = default;

  core::Result<T &> embedder() {
    if constexpr (std::is_same_v<T, FastTextEmbedder>) {
      if (std::holds_alternative<FastTextEmbedder>(embedder_)) {
        return core::Result<T &>::Ok(std::get<FastTextEmbedder>(embedder_));
      }
    }
    return core::Result<T &>::Error("Embedder not found or wrong type");
  }

  core::Result<bool> set(const std::string &model) {
    if constexpr (std::is_same_v<T, FastTextEmbedder>) {
      embedder_.emplace<FastTextEmbedder>();
      auto &embedder = std::get<FastTextEmbedder>(embedder_);
      embedder.loadModel(model);

      spdlog::info("Embedder initialized: {}", embedder.getEmbedderInfo());
      return core::Result<bool>::Ok(true);
    }

    return core::Result<bool>::Error("Unsupported embedder type");
  }

  EmbedderManager(const EmbedderManager &) = delete;
  EmbedderManager(EmbedderManager &&) = delete;
  EmbedderManager &operator=(const EmbedderManager &) = delete;
  EmbedderManager &operator=(EmbedderManager &&) = delete;

private:
  EmbedderVariant embedder_;
};

} // namespace owl

#endif // OWL_EMBEDDERD_MANAGER