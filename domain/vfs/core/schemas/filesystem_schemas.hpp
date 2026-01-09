#ifndef OWL_CORE_SCHEMAS_FILESYSTEM_SCHEMAS
#define OWL_CORE_SCHEMAS_FILESYSTEM_SCHEMAS

#include <boost/hana.hpp>

namespace owl {

struct ContainerMetadata {
  std::string container_id;
  std::string owner_id;
  std::string status;
  std::string data_path;
  std::vector<std::string> commands;
  std::map<std::string, std::string> labels;
  size_t memory_limit = 0;
  size_t storage_quota = 0;
  size_t file_limit = 0;
};

} // namespace owl

BOOST_HANA_ADAPT_STRUCT(owl::ContainerMetadata, container_id, owner_id, status,
                        data_path, commands, labels, memory_limit,
                        storage_quota, file_limit);

#endif // OWL_CORE_SCHEMAS_FILESYSTEM_SCHEMAS