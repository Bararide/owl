#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <locale>
#include <memory>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

namespace vfs::fileinfo {

class ScalarQuantizer {
private:
  std::vector<float> min_vals;
  std::vector<float> max_vals;
  std::vector<float> scales;
  std::vector<float> offsets;
  bool trained = false;

public:
  void train(const std::vector<std::vector<float>> &embeddings, size_t dim) {
    if (embeddings.empty()) {
      return;
    }

    min_vals.resize(dim, FLT_MAX);
    max_vals.resize(dim, -FLT_MAX);
    scales.resize(dim);
    offsets.resize(dim);

    for (const auto &embedding : embeddings) {
      for (size_t j = 0; j < dim; j++) {
        min_vals[j] = std::min(min_vals[j], embedding[j]);
        max_vals[j] = std::max(max_vals[j], embedding[j]);
      }
    }

    for (size_t j = 0; j < dim; j++) {
      float range = max_vals[j] - min_vals[j];
      if (range < 1e-10f) {
        scales[j] = 1.0f;
        offsets[j] = 0.0f;
      } else {
        scales[j] = 255.0f / range;
        offsets[j] = -min_vals[j] * scales[j];
      }
    }

    trained = true;
    spdlog::debug("SQ trained with {} dimensions", dim);
  }

  std::vector<uint8_t> quantize(const std::vector<float> &vec) {
    if (!trained) {
      throw std::runtime_error("SQ not trained");
    }

    std::vector<uint8_t> quantized(vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
      float quantized_val = vec[i] * scales[i] + offsets[i];
      quantized_val = std::max(0.0f, std::min(255.0f, quantized_val));
      quantized[i] = static_cast<uint8_t>(std::round(quantized_val));
    }
    return quantized;
  }

  std::vector<float> dequantize(const std::vector<uint8_t> &q_vec) {
    if (!trained) {
      throw std::runtime_error("SQ not trained");
    }

    std::vector<float> dequantized(q_vec.size());
    for (size_t i = 0; i < q_vec.size(); i++) {
      dequantized[i] = (static_cast<float>(q_vec[i]) - offsets[i]) / scales[i];
    }
    return dequantized;
  }

  float approximate_distance(const std::vector<uint8_t> &q_vec,
                             const std::vector<uint8_t> &db_vec) {
    float dist = 0;
    for (size_t i = 0; i < q_vec.size(); i++) {
      float q_val = (static_cast<float>(q_vec[i]) - offsets[i]) / scales[i];
      float db_val = (static_cast<float>(db_vec[i]) - offsets[i]) / scales[i];
      float diff = q_val - db_val;
      dist += diff * diff;
    }
    return std::sqrt(dist);
  }

  bool is_trained() const { return trained; }
};

class ProductQuantizer {
private:
  size_t m;
  size_t k;
  size_t d;
  size_t d_sub;

  std::vector<std::vector<std::vector<float>>> centroids;
  std::vector<std::vector<float>> precomputed_tables;
  bool trained = false;

  std::vector<std::vector<float>> kmeans(const std::vector<float> &data,
                                         size_t k, size_t dim,
                                         size_t max_iter = 100) {
    size_t n = data.size() / dim;
    if (n == 0)
      return {};

    std::vector<std::vector<float>> centroids(k, std::vector<float>(dim));
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), std::default_random_engine{});

    for (size_t i = 0; i < k; i++) {
      size_t idx = indices[i % n];
      for (size_t j = 0; j < dim; j++) {
        centroids[i][j] = data[idx * dim + j];
      }
    }

    std::vector<size_t> assignments(n);
    std::vector<std::vector<float>> new_centroids(k, std::vector<float>(dim));
    std::vector<size_t> counts(k);

    for (size_t iter = 0; iter < max_iter; iter++) {
      for (size_t i = 0; i < n; i++) {
        float min_dist = FLT_MAX;
        size_t best_centroid = 0;

        for (size_t j = 0; j < k; j++) {
          float dist = 0;
          for (size_t l = 0; l < dim; l++) {
            float diff = data[i * dim + l] - centroids[j][l];
            dist += diff * diff;
          }
          if (dist < min_dist) {
            min_dist = dist;
            best_centroid = j;
          }
        }
        assignments[i] = best_centroid;
      }

      std::fill(counts.begin(), counts.end(), 0);
      for (auto &centroid : new_centroids) {
        std::fill(centroid.begin(), centroid.end(), 0.0f);
      }

      for (size_t i = 0; i < n; i++) {
        size_t cluster = assignments[i];
        counts[cluster]++;
        for (size_t j = 0; j < dim; j++) {
          new_centroids[cluster][j] += data[i * dim + j];
        }
      }

      bool converged = true;
      for (size_t j = 0; j < k; j++) {
        if (counts[j] > 0) {
          for (size_t l = 0; l < dim; l++) {
            new_centroids[j][l] /= counts[j];
            if (std::abs(new_centroids[j][l] - centroids[j][l]) > 1e-6f) {
              converged = false;
            }
          }
        }
      }

      centroids = new_centroids;
      if (converged) {
        break;
      }
    }

    return centroids;
  }

public:
  ProductQuantizer(size_t m = 8, size_t k = 256) : m(m), k(k) {}

  void train(const std::vector<std::vector<float>> &embeddings, size_t dim) {
    if (embeddings.empty()) {
      return;
    }

    this->d = dim;
    this->d_sub = dim / m;
    if (d_sub * m != dim) {
      throw std::runtime_error("Dimension must be divisible by m");
    }

    centroids.resize(m);

    for (size_t i = 0; i < m; i++) {
      std::vector<float> subspace_data;
      for (const auto &embedding : embeddings) {
        subspace_data.insert(subspace_data.end(), embedding.begin() + i * d_sub,
                             embedding.begin() + (i + 1) * d_sub);
      }
      centroids[i] = kmeans(subspace_data, k, d_sub);
    }

    trained = true;
    spdlog::debug("PQ trained with m={}, k={}, d={}, d_sub={}", m, k, d, d_sub);
  }

  std::vector<uint8_t> encode(const std::vector<float> &vec) {
    if (!trained) {
      throw std::runtime_error("PQ not trained");
    }

    std::vector<uint8_t> codes(m);
    for (size_t i = 0; i < m; i++) {
      const float *subvec = vec.data() + i * d_sub;
      float min_dist = FLT_MAX;
      uint8_t best_code = 0;

      for (size_t j = 0; j < k; j++) {
        float dist = 0;
        for (size_t l = 0; l < d_sub; l++) {
          float diff = subvec[l] - centroids[i][j][l];
          dist += diff * diff;
        }
        if (dist < min_dist) {
          min_dist = dist;
          best_code = static_cast<uint8_t>(j);
        }
      }
      codes[i] = best_code;
    }
    return codes;
  }

  std::vector<float> decode(const std::vector<uint8_t> &codes) {
    if (!trained) {
      throw std::runtime_error("PQ not trained");
    }

    std::vector<float> reconstructed(d);
    for (size_t i = 0; i < m; i++) {
      size_t code = codes[i];
      for (size_t j = 0; j < d_sub; j++) {
        reconstructed[i * d_sub + j] = centroids[i][code][j];
      }
    }
    return reconstructed;
  }

  void precompute_query_tables(const std::vector<float> &query) {
    if (!trained) {
      throw std::runtime_error("PQ not trained");
    }

    precomputed_tables.resize(m, std::vector<float>(k));
    for (size_t i = 0; i < m; i++) {
      const float *query_sub = query.data() + i * d_sub;
      for (size_t j = 0; j < k; j++) {
        float dist = 0;
        for (size_t l = 0; l < d_sub; l++) {
          float diff = query_sub[l] - centroids[i][j][l];
          dist += diff * diff;
        }
        precomputed_tables[i][j] = dist;
      }
    }
  }

  float asymmetric_distance(const std::vector<uint8_t> &codes) {
    float dist = 0;
    for (size_t i = 0; i < m; i++) {
      dist += precomputed_tables[i][codes[i]];
    }
    return std::sqrt(dist);
  }

  bool is_trained() const { return trained; }
  size_t get_m() const { return m; }
  size_t get_k() const { return k; }
};

struct FileInfo {
  mode_t mode;
  size_t size;
  std::string content;
  uid_t uid;
  gid_t gid;
  time_t access_time;
  time_t modification_time;
  time_t create_time;
  std::vector<float> embedding;
  std::vector<uint8_t> pq_codes;
  std::vector<uint8_t> sq_codes;
  bool is_quantized = false;
  bool embedding_updated;

  FileInfo()
      : mode(0), size(0), uid(0), gid(0), access_time(time(nullptr)),
        modification_time(time(nullptr)), create_time(time(nullptr)),
        embedding_updated(false) {}

  FileInfo(mode_t mode, size_t size, const std::string &content, uid_t uid,
           gid_t gid, time_t access_time, time_t modification_time,
           time_t create_time)
      : mode(mode), size(size), content(content), uid(uid), gid(gid),
        access_time(access_time), modification_time(modification_time),
        create_time(create_time), embedding_updated(false) {}

  FileInfo(mode_t mode, size_t size, uid_t uid, gid_t gid, time_t access_time,
           time_t modification_time, time_t create_time)
      : mode(mode), size(size), uid(uid), gid(gid), access_time(access_time),
        modification_time(modification_time), create_time(create_time),
        embedding_updated(false) {}
};
} // namespace vfs::fileinfo

#endif // FILEINFO_HPP