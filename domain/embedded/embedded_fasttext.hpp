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

  void loadModel(const std::string &model_path) {
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
  getSentenceEmbedding(const std::vector<uint8_t> &text) {
    return core::Result<bool>::Ok(model_loaded_)
        .and_then([this](bool loaded) -> core::Result<bool> {
          if (!loaded) {
            return core::Result<bool>::Error("Model not loaded");
          }
          return core::Result<bool>::Ok(true);
        })
        .and_then([this, &text]() -> core::Result<std::vector<float>> {
          std::string text_str(text.begin(), text.end());
          std::istringstream iss(text_str);
          fasttext::Vector vec(dimension_);
          fasttext_->getSentenceVector(iss, vec);

          std::vector<float> embedding(dimension_);
          for (int i = 0; i < dimension_; ++i) {
            embedding[i] = vec[i];
          }
          return core::Result<std::vector<float>>::Ok(embedding);
        })
        .match(
            [](std::vector<float> embedding)
                -> core::Result<std::vector<float>> {
              return core::Result<std::vector<float>>::Ok(std::move(embedding));
            },
            [](const auto &error) -> core::Result<std::vector<float>> {
              spdlog::error("Failed to get sentence embedding: {}",
                            error.what());
              return core::Result<std::vector<float>>::Error(
                  "Embedding failed");
            });
  }

  int getDimensionImpl() const {
    validateModelLoaded();
    return dimension_;
  }

  std::string getModelNameImpl() const {
    return EmbedderTraits<FastTextEmbedder>::ModelName;
  }

  core::Result<schemas::FileInfo> handle(schemas::FileInfo &file) {
    return core::Result<schemas::FileInfo>::Ok(file)
        .and_then([this](schemas::FileInfo file_info)
                      -> core::Result<schemas::FileInfo> {
          if (!file_info.content.has_value()) {
            spdlog::error("File does not have content");
            return core::Result<schemas::FileInfo>::Ok(file_info);
          }

          return getSentenceEmbedding(file_info.content.value())
              .and_then([&file_info](std::vector<float> embedding)
                            -> core::Result<schemas::FileInfo> {
                spdlog::info("Created embedding for file {}",
                             file_info.name.value());
                return core::Result<schemas::FileInfo>::Ok(file_info);
              });
        })
        .match(
            [](schemas::FileInfo result) -> core::Result<schemas::FileInfo> {
              return core::Result<schemas::FileInfo>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<schemas::FileInfo> {
              spdlog::error("Embedding handling failed: {}", error.what());
              return core::Result<schemas::FileInfo>::Error(
                  "Embedding handling failed");
            });
  }

  void await() { spdlog::debug("await method in embedded"); }

  bool isModelLoadedImpl() const { return model_loaded_; }

private:
  std::unique_ptr<fasttext::FastText> fasttext_;
  std::string model_path_;
  int dimension_ = 0;
  bool model_loaded_ = false;
};
} // namespace owl::embedded

#endif // VECTORFS_EMBEDDED_FASTTEXT_HPP