#include "vectorfs.hpp"

namespace owl::vectorfs {
void VectorFS::rebuild_index() {
  if (!index_needs_rebuild)
    return;
  spdlog::info("Rebuilding vector index (quantization: {})", use_quantization);
  index_to_path.clear();
  std::vector<float> all_embeddings;
  std::vector<std::string> indexed_paths;
  idx_t idx = 0;
  for (const auto &[path, file_info] : virtual_files) {
    if (file_info.embedding_updated && !file_info.embedding.empty()) {
      all_embeddings.insert(all_embeddings.end(), file_info.embedding.begin(),
                            file_info.embedding.end());
      index_to_path[idx] = path;
      indexed_paths.push_back(path);
      idx++;
    }
  }
  if (!indexed_paths.empty()) {
    if (use_quantization) {
      if (!sq_quantizer->is_trained() || !pq_quantizer->is_trained()) {
        train_quantizers(all_embeddings,
                         std::visit(
                             [](const auto &embedder_ptr) {
                               return embedder_ptr->getDimension();
                             },
                             embedder_));
      }
      if (!faiss_index_quantized) {
        faiss_index_quantized = std::make_unique<faiss::IndexFlatL2>(std::visit(
            [](const auto &embedder_ptr) {
              return embedder_ptr->getDimension();
            },
            embedder_));
      } else {
        faiss_index_quantized->reset();
      }
      std::vector<uint8_t> pq_codes;
      for (const auto &path : indexed_paths) {
        const auto &file_info = virtual_files[path];
        if (!file_info.pq_codes.empty()) {
          pq_codes.insert(pq_codes.end(), file_info.pq_codes.begin(),
                          file_info.pq_codes.end());
        }
      }
      faiss_index_quantized->add(
          indexed_paths.size(),
          reinterpret_cast<const float *>(pq_codes.data()));
    } else {
      if (!faiss_index) {
        faiss_index = std::make_unique<faiss::IndexFlatL2>(std::visit(
            [](const auto &embedder_ptr) {
              return embedder_ptr->getDimension();
            },
            embedder_));
      } else {
        faiss_index->reset();
      }
      faiss_index->add(indexed_paths.size(), all_embeddings.data());
    }
    spdlog::info("Index rebuilt with {} files", indexed_paths.size());
    index_needs_rebuild = false;
  }
}

std::vector<float> VectorFS::get_embedding(const std::string &text) {
  return std::visit(
      [&text](auto &embedder_ptr) {
        return embedder_ptr->getSentenceEmbedding(text).unwrap();
      },
      embedder_);
}

std::string VectorFS::get_embedder_info() const {
  return std::visit(
      [](const auto &embedder_ptr) {
        return fmt::format("Model: {}, Dimension: {}",
                           embedder_ptr->getModelName(),
                           embedder_ptr->getDimension());
      },
      embedder_);
}

std::vector<std::pair<std::string, float>>
VectorFS::semantic_search(const std::string &query, int k) {
  std::vector<std::pair<std::string, float>> results;
  if (!std::holds_alternative<std::unique_ptr<embedded::FastTextEmbedder>>(
          embedder_) ||
      (!faiss_index && !faiss_index_quantized)) {
    spdlog::error("Embedder or index not initialized");
    return results;
  }
  rebuild_index();
  if (index_to_path.empty()) {
    spdlog::warn("No files indexed for search");
    return results;
  }
  std::string normalized_query = normalize_text(query);
  auto query_embedding = std::visit(
      [&normalized_query](auto &embedder_ptr) {
        return embedder_ptr->getSentenceEmbedding(normalized_query);
      },
      embedder_);
  std::vector<idx_t> I(k);
  std::vector<float> D(k);
  if (use_quantization && faiss_index_quantized) {
    if (pq_quantizer->is_trained()) {
      std::vector<uint8_t> query_codes = pq_quantizer->encode(query_embedding.unwrap());
      pq_quantizer->precompute_query_tables(query_embedding.unwrap());
      std::vector<std::pair<float, std::string>> scored_results;
      for (const auto &[idx, path] : index_to_path) {
        const auto &file_info = virtual_files[path];
        if (!file_info.pq_codes.empty()) {
          float dist = pq_quantizer->asymmetric_distance(file_info.pq_codes);
          scored_results.emplace_back(dist, path);
        }
      }
      std::sort(scored_results.begin(), scored_results.end());
      for (int i = 0; i < std::min(k, (int)scored_results.size()); ++i) {
        results.push_back({scored_results[i].second, scored_results[i].first});
      }
    }
  } else if (faiss_index) {
    faiss_index->search(1, query_embedding.unwrap().data(), k, D.data(), I.data());
    for (int i = 0; i < k; ++i) {
      if (I[i] >= 0 && index_to_path.find(I[i]) != index_to_path.end()) {
        results.push_back({index_to_path[I[i]], D[i]});
      }
    }
  }
  return results;
}

void VectorFS::update_semantic_relationships() {
  for (const auto &[path1, file1] : virtual_files) {
    if (!file1.embedding_updated || file1.embedding.empty())
      continue;

    for (const auto &[path2, file2] : virtual_files) {
      if (path1 == path2 || !file2.embedding_updated || file2.embedding.empty())
        continue;

      double similarity =
          calculate_cosine_similarity(file1.embedding, file2.embedding);

      if (similarity > 0.3) {
        semantic_graph->add_edge(path1, path2, similarity);
      }
    }
  }

  spdlog::info("Updated semantic relationships in graph");
}

void VectorFS::record_file_access(const std::string &file_path,
                                  const std::string &operation = "read") {
  semantic_graph->record_access(file_path, operation);

  recent_queries.push_back(file_path);
  if (recent_queries.size() > 50) {
    recent_queries.erase(recent_queries.begin(), recent_queries.begin() + 10);
  }

  if (recent_queries.size() >= 10) {
    std::vector<std::string> sequence(recent_queries.end() - 10,
                                      recent_queries.end());
    hmm_model->add_sequence(sequence);
  }

  auto now = std::chrono::steady_clock::now();
  auto time_since_update = std::chrono::duration_cast<std::chrono::minutes>(
                               now - last_ranking_update)
                               .count();

  if (time_since_update > 5) {
    update_models();
    last_ranking_update = now;
  }
}

void VectorFS::update_embedding(const std::string &path) {
  auto it = virtual_files.find(path);
  if (it == virtual_files.end())
    return;
  if (!it->second.content.empty()) {
    std::string normalized_content = normalize_text(it->second.content);
    it->second.embedding = std::visit(
        [&normalized_content](auto &embedder_ptr) {
          return embedder_ptr->getSentenceEmbedding(normalized_content).unwrap();
        },
        embedder_);
    it->second.embedding_updated = true;
    if (use_quantization && sq_quantizer && sq_quantizer->is_trained()) {
      it->second.sq_codes = sq_quantizer->quantize(it->second.embedding);
    }
    if (use_quantization && pq_quantizer && pq_quantizer->is_trained()) {
      it->second.pq_codes = pq_quantizer->encode(it->second.embedding);
    }
    index_needs_rebuild = true;
    spdlog::debug("Updated embedding for file: {}", path);
  }
}

std::string VectorFS::normalize_text(const std::string &text) {
  std::string result = text;
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

double VectorFS::calculate_cosine_similarity(const std::vector<float> &a,
                                             const std::vector<float> &b) {
  if (a.size() != b.size())
    return 0.0;

  double dot_product = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (size_t i = 0; i < a.size(); ++i) {
    dot_product += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  if (norm_a == 0.0 || norm_b == 0.0)
    return 0.0;

  return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

void VectorFS::update_models() {
  spdlog::info("Updating Random Walk rankings and HMM model");

  update_semantic_relationships();
  semantic_graph->random_walk_ranking();
  hmm_model->train();

  update_predictive_cache();
}

std::vector<std::string>
VectorFS::get_recommendations_for_query(const std::string &query) {
  auto search_results = semantic_search(query, 1);
  if (search_results.empty())
    return {};

  std::string current_file = search_results[0].first;
  return semantic_graph->get_recommendations(current_file, 3);
}

std::vector<std::string> VectorFS::predict_next_files() {
  return hmm_model->predict_next_files(recent_queries, 3);
}

void VectorFS::update_predictive_cache() {
  predictive_cache.clear();

  for (const auto &[file_path, _] : virtual_files) {
    auto recs = semantic_graph->get_recommendations(file_path, 5);
    if (!recs.empty()) {
      predictive_cache[file_path] = recs;
    }
  }

  spdlog::debug("Updated predictive cache with {} entries",
                predictive_cache.size());
}

std::string VectorFS::url_decode(const std::string &str) {
  std::string result;
  result.reserve(str.size());

  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%' && i + 2 < str.size()) {
      int value;
      std::istringstream iss(str.substr(i + 1, 2));
      if (iss >> std::hex >> value) {
        result += static_cast<char>(value);
        i += 2;
      } else {
        result += str[i];
      }
    } else if (str[i] == '+') {
      result += ' ';
    } else {
      result += str[i];
    }
  }

  return result;
}

void VectorFS::train_quantizers(const std::vector<float> &embeddings,
                                size_t dim) {
  if (embeddings.empty())
    return;

  std::vector<std::vector<float>> training_data;
  size_t num_vectors = embeddings.size() / dim;

  for (size_t i = 0; i < num_vectors; i++) {
    training_data.emplace_back(embeddings.begin() + i * dim,
                               embeddings.begin() + (i + 1) * dim);
  }

  sq_quantizer->train(training_data, dim);
  pq_quantizer->train(training_data, dim);

  spdlog::info("Quantizers trained with {} vectors", num_vectors);
}
} // namespace owl::vectorfs