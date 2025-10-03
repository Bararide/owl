#ifndef VECTORFS_EMBEDDED_FASTTEXT_HPP
#define VECTORFS_EMBEDDED_FASTTEXT_HPP

#include "embedded_base.hpp"
#include <fasttext.h>

namespace owl::embedded {
class FastTextEmbedder;

template <> struct EmbedderTraits<FastTextEmbedder> {
  using ModelType = fasttext::FastText;
  static constexpr const char *ModelName = "FastText";
  static constexpr bool SupportsBatchProcessing = false;
  static constexpr bool SupportsSubword = true;
};

class FastTextEmbedder : public EmbeddedBase<FastTextEmbedder> {
public:
  FastTextEmbedder() = default;
  explicit FastTextEmbedder(const std::string &model_path) {
    loadModel(model_path);
  }

  void loadModelImpl(const std::string &model_path) {
    model_path_ = model_path;
    fasttext_ = std::make_unique<fasttext::FastText>();
    fasttext_->loadModel(model_path_);
    dimension_ = fasttext_->getDimension();
    spdlog::info("FastText model loaded with dimension: {}", dimension_);
    model_loaded_ = true;
  }

  std::string getEmbedderInfo() const {
    return fmt::format("Model: {}, Dimension: {}", getModelName(),
                       getDimension());
  }

  core::Result<std::vector<float>>
  getSentenceEmbedding(const std::string &text) {
    validateModelLoaded();

    std::istringstream iss(text);
    fasttext::Vector vec(dimension_);
    fasttext_->getSentenceVector(iss, vec);

    std::vector<float> embedding(dimension_);
    for (int i = 0; i < dimension_; ++i) {
      embedding[i] = vec[i];
    }
    return core::Result<std::vector<float>>::Ok(embedding);
  }

  int getDimensionImpl() const {
    validateModelLoaded();
    return dimension_;
  }

  std::string getModelNameImpl() const {
    return EmbedderTraits<FastTextEmbedder>::ModelName;
  }

  core::Result<schemas::FileInfo> handle(const schemas::FileInfo &file) {
    if (file.content.has_value()) {
      auto result = getSentenceEmbedding(file.content.value());

      if (result.is_ok()) {
        spdlog::info("create embedding for file {} success", file.name.value());
      } else {
        spdlog::error("create embedding fault");
      }
    } else {
      spdlog::error("file not have content");
    }

    return core::Result<schemas::FileInfo>::Ok(file);
  }

  void await() {
    spdlog::debug("await method in embedded");
  }

  bool isModelLoadedImpl() const { return model_loaded_; }

private:
  std::unique_ptr<fasttext::FastText> fasttext_;
  std::string model_path_;
  int dimension_ = 0;
  bool model_loaded_ = false;
};
} // namespace owl::embedded

#endif // VECTORFS_EMBEDDED_FASTTEXT_HPP