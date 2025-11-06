#ifndef VECTORFS_KNOWLEDGE_CONTAINER_HPP
#define VECTORFS_KNOWLEDGE_CONTAINER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace owl::vectorfs {

template<typename Derived>
class ContainerOwner {
  public:

};

class IKnowledgeContainer {
public:
  virtual ~IKnowledgeContainer() = default;
  virtual std::string get_id() const = 0;
  virtual std::string get_owner() const = 0;
  virtual std::string get_namespace() const = 0;
  virtual std::vector<std::string> get_commands() const = 0;
  virtual std::map<std::string, std::string> get_labels() const = 0;
  virtual std::vector<std::string>
  list_files(const std::string &path = "/") const = 0;

  virtual std::string get_file_content(const std::string &path) const = 0;
  virtual bool add_file(const std::string &path,
                        const std::string &content) = 0;
  virtual bool remove_file(const std::string &path) = 0;
  virtual bool is_directory(const std::string &virtual_path) const = 0;

  virtual bool file_exists(const std::string &path) const = 0;
  virtual std::vector<std::pair<std::string, float>>
  semantic_search(const std::string &query, int limit = 10) = 0;
  virtual std::vector<std::string>
  search_files(const std::string &pattern) const = 0;
  virtual bool is_available() const = 0;
  virtual size_t get_size() const = 0;
  virtual std::string get_status() const = 0;
  virtual std::string get_data_path() const = 0;

  virtual std::vector<std::pair<std::string, float>>
  enhanced_semantic_search(const std::string &query, int limit = 10) = 0;

  virtual std::vector<std::string>
  get_recommendations(const std::string &current_file, int limit = 5) = 0;

  virtual std::vector<std::string> predict_next_files(int limit = 3) = 0;

  virtual std::vector<std::string> get_semantic_hubs(int count = 5) = 0;

  virtual std::string classify_file(const std::string &file_path) = 0;

  virtual bool update_all_embeddings() = 0;

  virtual std::string get_search_info() const = 0;

  virtual void record_search_query(const std::string &query) = 0;
};

} // namespace owl::vectorfs

#endif