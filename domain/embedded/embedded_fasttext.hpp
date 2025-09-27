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

  std::vector<float> getSentenceEmbeddingImpl(const std::string &text) {
    validateModelLoaded();

    std::istringstream iss(text);
    fasttext::Vector vec(dimension_);
    fasttext_->getSentenceVector(iss, vec);

    std::vector<float> embedding(dimension_);
    for (int i = 0; i < dimension_; ++i) {
      embedding[i] = vec[i];
    }
    return embedding;
  }

  int getDimensionImpl() const {
    validateModelLoaded();
    return dimension_;
  }

  std::string getModelNameImpl() const {
    return EmbedderTraits<FastTextEmbedder>::ModelName;
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