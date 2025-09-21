#ifndef VECTORFS_EMBEDDED_UTILS_HPP
#define VECTORFS_EMBEDDED_UTILS_HPP

#include <concepts>
#include <fasttext.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace owl::embedded::utils {
template <typename EmbedderType> class EmbedderFactory {
public:
  static std::unique_ptr<EmbedderType>
  create(const std::string &model_path = "") {
    auto embedder = std::make_unique<EmbedderType>();
    if (!model_path.empty()) {
      embedder->loadModel(model_path);
    }
    return embedder;
  }
};

template <IsEmbedder Embedder>
float cosineSimilarity(const std::vector<float> &vec1,
                       const std::vector<float> &vec2,
                       const Embedder &embedder) {
  if (vec1.size() != vec2.size() || vec1.size() != embedder.getDimension()) {
    throw std::invalid_argument("Vector dimensions mismatch");
  }

  float dot = 0.0f;
  float norm1 = 0.0f;
  float norm2 = 0.0f;

  for (size_t i = 0; i < vec1.size(); ++i) {
    dot += vec1[i] * vec2[i];
    norm1 += vec1[i] * vec1[i];
    norm2 += vec2[i] * vec2[i];
  }

  return dot / (std::sqrt(norm1) * std::sqrt(norm2));
}

template <typename T> void normalize(std::vector<T> &vector) {
  T norm = 0.0;
  for (const auto &val : vector) {
    norm += val * val;
  }
  norm = std::sqrt(norm);

  if (norm > 0) {
    for (auto &val : vector) {
      val /= norm;
    }
  }
}
} // namespace owl::embedded::utils

// namespace owl::embedded

#endif // VECTORFS_EMBEDDED_UTILS_HPP
