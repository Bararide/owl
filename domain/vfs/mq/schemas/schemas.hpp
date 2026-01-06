#ifndef OWL_MQ_SCHEMAS
#define OWL_MQ_SCHEMAS

#include <boost/hana.hpp>
#include <boost/preprocessor.hpp>
#include <string>
#include <vector>

namespace owl {

struct ContainerCreateSchema {
  std::string request_id;
  std::string container_id;
  std::string user_id;
  std::string memory_limit;
  std::string storage_quota;
  int file_limit;
  bool privileged;
  std::string env_label;
  std::string type_label;
  std::vector<std::string> commands;
};

struct ContainerUserSchema {
  std::string request_id;
  std::string container_id;
  std::string user_id;
};

struct ContainerGetFilesSchema {
  std::string request_id;
  std::string container_id;
  std::string user_id;
  bool rebuild_index = false;
};

struct ContainerSchema {
  std::string request_id;
  std::string container_id;
};

struct FileCreateSchema {
  std::string request_id;
  std::string path;
  std::string content;
  std::string user_id;
  std::string container_id;
};

struct FileDeleteSchema {
  std::string request_id;
  std::string path;
  std::string user_id;
  std::string container_id;
};

struct SemanticSearchSchema {
  std::string request_id;
  std::string query;
  int limit = 10;
  std::string user_id;
  std::string container_id;
};

struct SemanticSearchGlobalSchema {
  std::string request_id;
  std::string query;
  int limit = 10;
};

struct ContainerStopSchema {
  std::string request_id;
  std::string container_id;
};

struct ContainerDeleteSchema {
  std::string request_id;
  std::string container_id;
};

} // namespace owl

BOOST_HANA_ADAPT_STRUCT(owl::ContainerCreateSchema, request_id, container_id,
                        user_id, memory_limit, storage_quota, file_limit,
                        privileged, env_label, type_label, commands);
BOOST_HANA_ADAPT_STRUCT(owl::ContainerUserSchema, request_id, container_id,
                        user_id);
BOOST_HANA_ADAPT_STRUCT(owl::ContainerGetFilesSchema, request_id, container_id,
                        user_id, rebuild_index);
BOOST_HANA_ADAPT_STRUCT(owl::ContainerSchema, request_id, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::FileCreateSchema, request_id, path, content,
                        user_id, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::FileDeleteSchema, request_id, path, user_id,
                        container_id);
BOOST_HANA_ADAPT_STRUCT(owl::SemanticSearchSchema, request_id, query, limit,
                        user_id, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::SemanticSearchGlobalSchema, request_id, query,
                        limit);
BOOST_HANA_ADAPT_STRUCT(owl::ContainerStopSchema, request_id, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::ContainerDeleteSchema, request_id, container_id);

#endif // OWL_MQ_SCHEMAS