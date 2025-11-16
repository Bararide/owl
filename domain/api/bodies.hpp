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

struct SemanticSearchInContainer {
  std::string query;
  int limit;
  std::string user_id;
  std::string container_id;
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

struct DeleteContainer {
  std::string user_id;
  std::string container_id;
};

struct DeleteFile {
  std::string user_id;
  std::string container_id;
  std::string file_id;
};

struct ContainerFiles {
  std::string user_id;
  std::string container_id;
};

struct ReadFileByIdBody {
  std::string file_id;
  std::string container_id;
};

} // namespace owl::api::validate

BOOST_HANA_ADAPT_STRUCT(owl::api::validate::SemanticSearch, query, limit);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::SemanticSearchInContainer, query,
                        limit, user_id, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::CreateFile, path, content, user_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::CreateContainer, user_id,
                        container_id, memory_limit, storage_quota, file_limit,
                        env_label, type_label, commands, privileged);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::ReadFileByIdBody, file_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::DeleteContainer, user_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::ContainerFiles, user_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::api::validate::DeleteFile, user_id,
                        container_id, file_id);

#endif // OWL_API_BODIES