#ifndef EMBEDDED_HPP
#define EMBEDDED_HPP

#include <fasttext.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <vector>

namespace embedded {
class FastTextEmbedder {
private:
  std::unique_ptr<fasttext::FastText> fasttext_;
  std::string model_path_;
  int dimension_;

public:
  FastTextEmbedder(const std::string &model_path) : model_path_(model_path) {
    fasttext_ = std::make_unique<fasttext::FastText>();
    fasttext_->loadModel(model_path_);
    dimension_ = fasttext_->getDimension();
    spdlog::info("FastText model loaded with dimension: {}", dimension_);
  }

  std::vector<float> getSentenceEmbedding(const std::string &text) {
    std::istringstream iss(text);
    fasttext::Vector vec(dimension_);
    fasttext_->getSentenceVector(iss, vec);

    std::vector<float> embedding(dimension_);
    for (int i = 0; i < dimension_; ++i) {
      embedding[i] = vec[i];
    }
    return embedding;
  }

  int getDimension() const { return dimension_; }
};
} // namespace embedded

#endif // EMBEDDED_HPP