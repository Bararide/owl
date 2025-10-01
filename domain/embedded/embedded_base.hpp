#ifndef EMBEDDED_BASE_HPP
#define EMBEDDED_BASE_HPP

#include "schemas/fileinfo.hpp"
#include <concepts>
#include <infrastructure/result.hpp>
#include <memory>
#include <pipeline/pipeline.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace owl::embedded {

template <typename Derived> class EmbeddedBase;

template <typename T> struct EmbedderTraits {
  using ModelType = void;
  static constexpr const char *ModelName = "Unknown";
  static constexpr bool SupportsBatchProcessing = false;
  static constexpr bool SupportsSubword = false;
  static constexpr bool SupportsPrediction = false;
};

template <typename Derived>
class EmbeddedBase
    : public core::pipeline::PipelineHandler<Derived, schemas::FileInfo> {
public:
  void loadModel(const std::string &model_path) {
    static_cast<Derived *>(this)->loadModelImpl(model_path);
  }

  std::vector<float> getSentenceEmbedding(const std::string &text) {
    return static_cast<Derived *>(this)->getSentenceEmbeddingImpl(text);
  }

  int getDimension() const {
    return static_cast<const Derived *>(this)->getDimensionImpl();
  }

  std::string getModelName() const {
    return static_cast<const Derived *>(this)->getModelNameImpl();
  }

  bool isModelLoaded() const {
    return static_cast<const Derived *>(this)->isModelLoadedImpl();
  }

  std::string getEmbedderInfo() const {
    return static_cast<const Derived *>(this)->getEmbedderInfo();
  }

  core::Result<schemas::FileInfo> handle(const schemas::FileInfo &file) {
    return static_cast<Derived *>(this)->handle(file);
  }

  void await() {
    if constexpr (requires(Derived d) { d.await(); }) {
      static_cast<Derived *>(this)->await();
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  virtual ~EmbeddedBase() = default;

protected:
  void validateModelLoaded() const {
    if (!isModelLoaded()) {
      throw std::runtime_error("Model is not loaded");
    }
  }
};

} // namespace owl::embedded

#endif // EMBEDDED_BASE_HPP