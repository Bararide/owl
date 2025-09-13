#ifndef MARKOV_RECOMMENDER_HPP
#define MARKOV_RECOMMENDER_HPP

#include <algorithm>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

namespace vfs::markov {

struct Edge {
  std::string target;
  double weight;
  double semantic_similarity;
  int usage_count;

  Edge(const std::string &t, double w, double sim = 0.0)
      : target(t), weight(w), semantic_similarity(sim), usage_count(1) {}
};

struct AccessPattern {
  std::string file_path;
  std::chrono::time_point<std::chrono::steady_clock> timestamp;
  std::string context;
};

class SemanticGraph {
private:
  std::unordered_map<std::string, std::vector<Edge>> adjacency_list;
  std::unordered_map<std::string, double> node_importance;
  std::vector<AccessPattern> access_history;
  std::random_device rd;
  std::mt19937 gen;

  static constexpr double DAMPING_FACTOR = 0.85;
  static constexpr double CONVERGENCE_THRESHOLD = 1e-6;
  static constexpr int MAX_ITERATIONS = 100;
  static constexpr int WALK_LENGTH = 50;

public:
  SemanticGraph() : gen(rd()) {}

  void add_edge(const std::string &from, const std::string &to,
                double semantic_similarity, int usage_weight = 1) {
    double edge_weight = semantic_similarity * (1.0 + std::log(usage_weight));

    auto &edges = adjacency_list[from];
    auto it = std::find_if(edges.begin(), edges.end(),
                           [&to](const Edge &e) { return e.target == to; });

    if (it != edges.end()) {
      it->weight = (it->weight + edge_weight) / 2.0;
      it->semantic_similarity =
          std::max(it->semantic_similarity, semantic_similarity);
      it->usage_count += usage_weight;
    } else {
      edges.emplace_back(to, edge_weight, semantic_similarity);
    }

    normalize_outgoing_weights(from);
  }

  void record_access(const std::string &file_path,
                     const std::string &context = "") {
    access_history.push_back(
        {file_path, std::chrono::steady_clock::now(), context});

    if (access_history.size() > 1000) {
      access_history.erase(access_history.begin(),
                           access_history.begin() + 100);
    }

    update_transition_probabilities();
  }

  std::vector<std::pair<std::string, double>>
  random_walk_ranking(int num_walks = 1000) {
    std::unordered_map<std::string, int> visit_counts;

    std::vector<std::string> nodes;
    for (const auto &[node, _] : adjacency_list) {
      nodes.push_back(node);
    }

    if (nodes.empty())
      return {};

    std::uniform_int_distribution<> node_dist(0, nodes.size() - 1);

    for (int walk = 0; walk < num_walks; ++walk) {
      std::string current = nodes[node_dist(gen)];

      for (int step = 0; step < WALK_LENGTH; ++step) {
        visit_counts[current]++;

        auto it = adjacency_list.find(current);
        if (it == adjacency_list.end() || it->second.empty()) {
          current = nodes[node_dist(gen)];
          continue;
        }

        current = select_next_node(it->second);
      }
    }

    std::vector<std::pair<std::string, double>> ranking;
    double total_visits = num_walks * WALK_LENGTH;

    for (const auto &[node, count] : visit_counts) {
      double importance = static_cast<double>(count) / total_visits;
      ranking.emplace_back(node, importance);
    }

    std::sort(ranking.begin(), ranking.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    for (const auto &[node, score] : ranking) {
      node_importance[node] = score;
    }

    return ranking;
  }

  std::vector<std::string> get_recommendations(const std::string &current_file,
                                               int num_recommendations = 5) {
    std::vector<std::string> recommendations;

    auto it = adjacency_list.find(current_file);
    if (it == adjacency_list.end()) {
      return recommendations;
    }

    std::vector<std::pair<std::string, double>> candidates;

    for (const auto &edge : it->second) {
      double score = edge.weight;

      auto importance_it = node_importance.find(edge.target);
      if (importance_it != node_importance.end()) {
        score *= (1.0 + importance_it->second);
      }

      score *= (1.0 + edge.semantic_similarity);

      candidates.emplace_back(edge.target, score);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    for (int i = 0; i < std::min(num_recommendations, (int)candidates.size());
         ++i) {
      recommendations.push_back(candidates[i].first);
    }

    return recommendations;
  }

  double get_transition_probability(const std::string &from,
                                    const std::string &to) const {
    auto it = adjacency_list.find(from);
    if (it == adjacency_list.end())
      return 0.0;

    for (const auto &edge : it->second) {
      if (edge.target == to) {
        return edge.weight;
      }
    }
    return 0.0;
  }

  std::vector<std::string> get_semantic_hubs(int top_k = 10) {
    std::vector<std::pair<std::string, double>> hub_scores;

    for (const auto &[node, _] : adjacency_list) {
      double hub_score = calculate_hub_score(node);
      hub_scores.emplace_back(node, hub_score);
    }

    std::sort(hub_scores.begin(), hub_scores.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<std::string> hubs;
    for (int i = 0; i < std::min(top_k, (int)hub_scores.size()); ++i) {
      hubs.push_back(hub_scores[i].first);
    }

    return hubs;
  }

  size_t get_node_count() const { return adjacency_list.size(); }

  size_t get_edge_count() const {
    size_t count = 0;
    for (const auto &[node, edges] : adjacency_list) {
      count += edges.size();
    }
    return count;
  }

private:
  void normalize_outgoing_weights(const std::string &node) {
    auto it = adjacency_list.find(node);
    if (it == adjacency_list.end())
      return;

    double total_weight = 0.0;
    for (const auto &edge : it->second) {
      total_weight += edge.weight;
    }

    if (total_weight > 0) {
      for (auto &edge : it->second) {
        edge.weight /= total_weight;
      }
    }
  }

  std::string select_next_node(const std::vector<Edge> &edges) {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    double rand_val = dist(gen);
    double cumulative = 0.0;

    for (const auto &edge : edges) {
      cumulative += edge.weight;
      if (rand_val <= cumulative) {
        return edge.target;
      }
    }

    return edges.back().target;
  }

  void update_transition_probabilities() {
    if (access_history.size() < 2)
      return;

    int window_size = std::min(10, (int)access_history.size());
    auto recent_start = access_history.end() - window_size;

    for (auto it = recent_start; it != access_history.end() - 1; ++it) {
      auto next_it = it + 1;

      auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                           next_it->timestamp - it->timestamp)
                           .count();

      if (time_diff < 300) {
        double temporal_weight = 1.0 / (1.0 + time_diff / 60.0);

        add_edge(it->file_path, next_it->file_path, 0.5,
                 static_cast<int>(temporal_weight * 10));
      }
    }
  }

  double calculate_hub_score(const std::string &node) {
    auto it = adjacency_list.find(node);
    if (it == adjacency_list.end())
      return 0.0;

    double out_degree = it->second.size();

    double in_degree = 0;
    for (const auto &[other_node, edges] : adjacency_list) {
      for (const auto &edge : edges) {
        if (edge.target == node) {
          in_degree++;
        }
      }
    }

    double avg_similarity = 0.0;
    if (!it->second.empty()) {
      for (const auto &edge : it->second) {
        avg_similarity += edge.semantic_similarity;
      }
      avg_similarity /= it->second.size();
    }

    double pagerank_score = 0.0;
    auto pr_it = node_importance.find(node);
    if (pr_it != node_importance.end()) {
      pagerank_score = pr_it->second;
    }

    return (in_degree + out_degree) * avg_similarity * (1.0 + pagerank_score);
  }
};

class HiddenMarkovModel {
private:
  std::vector<std::string> states;
  std::vector<std::string> observations;
  std::map<std::string, int> state_to_index;
  std::map<std::string, int> obs_to_index;

  std::vector<std::vector<double>> transition_matrix;
  std::vector<std::vector<double>> emission_matrix;
  std::vector<double> initial_probs;

  std::vector<std::vector<std::string>> observation_sequences;

public:
  size_t get_state_count() const { return states.size(); }
  size_t get_sequence_count() const { return observation_sequences.size(); }

  void add_state(const std::string &state) {
    if (state_to_index.find(state) == state_to_index.end()) {
      state_to_index[state] = states.size();
      states.push_back(state);
    }
  }

  void add_observation(const std::string &obs) {
    if (obs_to_index.find(obs) == obs_to_index.end()) {
      obs_to_index[obs] = observations.size();
      observations.push_back(obs);
    }
  }

  void add_sequence(const std::vector<std::string> &sequence) {
    observation_sequences.push_back(sequence);

    for (const auto &obs : sequence) {
      add_observation(obs);
    }
  }

  void train() {
    if (states.empty() || observations.empty()) {
      spdlog::error("HMM: No states or observations defined");
      return;
    }

    size_t num_states = states.size();
    size_t num_obs = observations.size();

    transition_matrix.assign(num_states, std::vector<double>(num_states, 0.0));
    emission_matrix.assign(num_states, std::vector<double>(num_obs, 0.0));
    initial_probs.assign(num_states, 1.0 / num_states);

    train_from_sequences();

    spdlog::info("HMM trained with {} states and {} observations", num_states,
                 num_obs);
  }

  std::vector<std::string>
  predict_next_files(const std::vector<std::string> &recent_files,
                     int num_predictions = 3) {
    if (recent_files.empty() || !is_trained()) {
      return {};
    }

    std::vector<double> state_probs = get_state_probabilities(recent_files);
    std::vector<std::pair<std::string, double>> predictions;

    for (size_t s = 0; s < states.size(); ++s) {
      for (size_t o = 0; o < observations.size(); ++o) {
        double prob = state_probs[s] * emission_matrix[s][o];
        if (prob > 0.01) {
          predictions.emplace_back(observations[o], prob);
        }
      }
    }

    std::sort(predictions.begin(), predictions.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (int i = 0; i < std::min(num_predictions, (int)predictions.size());
         ++i) {
      result.push_back(predictions[i].first);
    }

    return result;
  }

  std::string
  classify_file_category(const std::string &file_path,
                         const std::vector<std::string> &context_files) {
    if (!is_trained())
      return "unknown";

    std::vector<double> state_probs = get_state_probabilities(context_files);

    auto obs_it = obs_to_index.find(file_path);
    if (obs_it != obs_to_index.end()) {
      int obs_idx = obs_it->second;

      double max_prob = 0.0;
      int best_state = 0;

      for (size_t s = 0; s < states.size(); ++s) {
        double prob = state_probs[s] * emission_matrix[s][obs_idx];
        if (prob > max_prob) {
          max_prob = prob;
          best_state = s;
        }
      }

      return states[best_state];
    }

    auto max_it = std::max_element(state_probs.begin(), state_probs.end());
    return states[std::distance(state_probs.begin(), max_it)];
  }

private:
  bool is_trained() const {
    return !transition_matrix.empty() && !emission_matrix.empty();
  }

  void train_from_sequences() {
    std::vector<std::vector<int>> trans_counts(
        states.size(), std::vector<int>(states.size(), 0));
    std::vector<std::vector<int>> emit_counts(
        states.size(), std::vector<int>(observations.size(), 0));
    std::vector<int> state_counts(states.size(), 0);

    for (const auto &sequence : observation_sequences) {
      for (size_t i = 0; i < sequence.size(); ++i) {
        std::string current_state = infer_state(sequence[i]);
        int state_idx = state_to_index[current_state];
        int obs_idx = obs_to_index[sequence[i]];

        emit_counts[state_idx][obs_idx]++;
        state_counts[state_idx]++;

        if (i > 0) {
          std::string prev_state = infer_state(sequence[i - 1]);
          int prev_state_idx = state_to_index[prev_state];
          trans_counts[prev_state_idx][state_idx]++;
        }
      }
    }

    for (size_t i = 0; i < states.size(); ++i) {
      int total_trans =
          std::accumulate(trans_counts[i].begin(), trans_counts[i].end(), 0);
      if (total_trans > 0) {
        for (size_t j = 0; j < states.size(); ++j) {
          transition_matrix[i][j] =
              static_cast<double>(trans_counts[i][j]) / total_trans;
        }
      }

      if (state_counts[i] > 0) {
        for (size_t j = 0; j < observations.size(); ++j) {
          emission_matrix[i][j] =
              static_cast<double>(emit_counts[i][j]) / state_counts[i];
        }
      }
    }
  }

  std::string infer_state(const std::string &file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
      std::string ext = file_path.substr(dot_pos + 1);
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (ext == "cpp" || ext == "hpp" || ext == "c" || ext == "h") {
        return "code";
      } else if (ext == "txt" || ext == "md" || ext == "doc") {
        return "document";
      } else if (ext == "json" || ext == "xml" || ext == "yaml") {
        return "config";
      } else if (ext == "py" || ext == "js" || ext == "java") {
        return "script";
      }
    }

    if (file_path.find("test") != std::string::npos) {
      return "test";
    } else if (file_path.find("doc") != std::string::npos) {
      return "document";
    } else if (file_path.find("config") != std::string::npos ||
               file_path.find("conf") != std::string::npos) {
      return "config";
    }

    return "misc";
  }

  std::vector<double>
  get_state_probabilities(const std::vector<std::string> &observations_seq) {
    std::vector<double> probs(states.size(), 1.0 / states.size());

    if (observations_seq.empty() || !is_trained()) {
      return probs;
    }

    for (const auto &obs : observations_seq) {
      auto obs_it = obs_to_index.find(obs);
      if (obs_it == obs_to_index.end())
        continue;

      int obs_idx = obs_it->second;
      std::vector<double> new_probs(states.size(), 0.0);

      for (size_t j = 0; j < states.size(); ++j) {
        for (size_t i = 0; i < states.size(); ++i) {
          new_probs[j] +=
              probs[i] * transition_matrix[i][j] * emission_matrix[j][obs_idx];
        }
      }

      double sum = std::accumulate(new_probs.begin(), new_probs.end(), 0.0);
      if (sum > 0) {
        for (auto &p : new_probs) {
          p /= sum;
        }
      }

      probs = new_probs;
    }

    return probs;
  }
};

} // namespace vfs::markov

#endif // MARKOV_RECOMMENDER_HPP