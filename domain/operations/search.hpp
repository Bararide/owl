#ifndef CONTAINER_SEARCH_HPP
#define CONTAINER_SEARCH_HPP

#include <string>
#include <vector>

namespace owl::vectorfs {

template <typename Derived> class SearchOperations {
public:
  std::vector<std::pair<std::string, float>>
  semanticSearch(const std::string &query, int limit = 10) {
    return static_cast<Derived *>(this)->semanticSearch(query, limit);
  }

  std::vector<std::pair<std::string, float>>
  enhancedSemanticSearch(const std::string &query, int limit = 10) {
    return static_cast<Derived *>(this)->enhancedSemanticSearch(query,
                                                                       limit);
  }

  std::vector<std::string> getRecommendations(const std::string &current_file,
                                               int limit = 5) {
    return static_cast<Derived *>(this)->getRecommendations(current_file,
                                                                  limit);
  }

  std::vector<std::string> predictNextFiles(int limit = 3) {
    return static_cast<Derived *>(this)->predictNextFiles(limit);
  }

  std::vector<std::string> getSemanticHubs(int count = 5) {
    return static_cast<Derived *>(this)->getSemanticHubs(count);
  }

  std::string classifyFile(const std::string &file_path) {
    return static_cast<Derived *>(this)->classifyFile(file_path);
  }

  void recordSearchQuery(const std::string &query) {
    static_cast<Derived *>(this)->recordSearchQuery(query);
  }
};

} // namespace owl::vectorfs

#endif // CONTAINER_SEARCH_HPP