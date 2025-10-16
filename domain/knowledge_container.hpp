#ifndef VECTORFS_KNOWLEDGE_CONTAINER_HPP
#define VECTORFS_KNOWLEDGE_CONTAINER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace owl::vectorfs {

class IKnowledgeContainer {
public:
  virtual ~IKnowledgeContainer() = default;

  virtual std::string get_id() const = 0;
  virtual std::string get_owner() const = 0;
  virtual std::string get_namespace() const = 0;
  virtual std::map<std::string, std::string> get_labels() const = 0;

  virtual std::vector<std::string>
  list_files(const std::string &path = "/") const = 0;
  virtual std::string read_file(const std::string &path) const = 0;
  virtual bool write_file(const std::string &path,
                          const std::string &content) = 0;
  virtual bool file_exists(const std::string &path) const = 0;

  virtual std::vector<std::string> semantic_search(const std::string &query,
                                                   int limit = 10) const = 0;
  virtual std::vector<std::string>
  search_files(const std::string &pattern) const = 0;

  virtual bool is_available() const = 0;
  virtual size_t get_size() const = 0;
  virtual std::string get_status() const = 0;
};

} // namespace owl::vectorfs

#endif // VECTORFS_KNOWLEDGE_CONTAINER_HPP