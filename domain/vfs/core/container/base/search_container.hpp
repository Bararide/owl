#ifndef OWL_VFS_CORE_CONTAINER_SEARCH_CONTAINER
#define OWL_VFS_CORE_CONTAINER_SEARCH_CONTAINER

#include <infrastructure/result.hpp>

namespace owl {

template <typename Derived> class SearchableContainer {
public:
  core::Result<std::vector<std::pair<std::string, float>>>
  semanticSearch(const std::string &query, int limit = 10) {
    return derived().semanticSearch(query, limit);
  }

  core::Result<std::vector<std::pair<std::string, float>>>
  enhancedSemanticSearch(const std::string &query, int limit = 10) {
    return derived().enhancedSemanticSearch(query, limit);
  }

  core::Result<std::vector<std::string>>
  getRecommendations(const std::string &current_file, int limit = 5) {
    return derived().getRecommendations(current_file, limit);
  }

  core::Result<std::vector<std::string>> predictNextFiles(int limit = 3) {
    return derived().predictNextFiles(limit);
  }

  core::Result<std::vector<std::string>> getSemanticHubs(int count = 5) {
    return derived().getSemanticHubs(count);
  }

  core::Result<std::string> classifyFile(const std::string &file_path) {
    return derived().classifyFile(file_path);
  }

  core::Result<void> updateAllEmbeddings() {
    return derived().updateAllEmbeddings();
  }

  core::Result<std::string> getSearchInfo() const {
    return derived().getSearchInfo();
  }

  core::Result<void> recordSearchQuery(const std::string &query) {
    return derived().recordSearchQuery(query);
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_SEARCH_CONTAINER