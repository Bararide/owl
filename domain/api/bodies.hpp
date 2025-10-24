#ifndef OWL_API_BODIES
#define OWL_API_BODIES

#include <boost/hana.hpp>
#include <string>
#include <utility>
#include <vector>

namespace owl::api::validate {

namespace hana = boost::hana;

struct SemanticSearch {
  std::string query;
  int limit;
};

struct CreateFile {
  std::string path;
  std::string content;
  std::string user_id;
  std::string container_id;
};

struct CreateContainer {
  std::string user_id;
  std::string container_id;
  size_t memory_limit;
  size_t storage_quota;
  size_t file_limit;
  std::pair<std::string, std::string> env_label;
  std::pair<std::string, std::string> type_label;
  std::vector<std::string> commands;
  bool privileged;
};

} // namespace owl::api::validate

BOOST_HANA_ADAPT_STRUCT(owl::api::validate::SemanticSearch, query, limit);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::CreateFile, path, content, user_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::CreateContainer, user_id,
                        container_id, memory_limit, storage_quota, file_limit,
                        env_label, type_label, commands, privileged);

#endif // OWL_API_BODIES