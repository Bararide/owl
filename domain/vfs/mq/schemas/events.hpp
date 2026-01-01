#ifndef OWL_VFS_MQ_SCHEMAS_EVENTS
#define OWL_VFS_MQ_SCHEMAS_EVENTS

#include <boost/fusion/functional.hpp>
#include <boost/hana.hpp>
#include <nlohmann/json.hpp>

namespace owl {

struct BaseEvent {
  std::string request_id;
  std::string type;
  nlohmann::json data;

  virtual ~BaseEvent() = default;
};

struct ContainerCreateEvent : BaseEvent {
  std::string container_id;
  std::string user_id;
  size_t memory_limit;
  size_t storage_quota;
  size_t file_limit;
  bool privileged;
  std::string env_label;
  std::string type_label;
  std::vector<std::string> commands;
};

struct GetContainerFilesEvent : BaseEvent {
  std::string container_id;
  std::string user_id;
  bool rebuild_index = false;
};

struct ContainerDeleteEvent : BaseEvent {
  std::string container_id;
};

struct FileCreateEvent : BaseEvent {
  std::string path;
  std::string content;
  std::string user_id;
  std::string container_id;
};

struct FileDeleteEvent : BaseEvent {
  std::string path;
  std::string user_id;
  std::string container_id;
};

struct SemanticSearchEvent : BaseEvent {
  std::string query;
  int limit = 10;
  std::string user_id;
  std::string container_id;
};

struct ContainerStopEvent : BaseEvent {
  std::string container_id;
};

} // namespace owl

// BOOST_HANA_ADAPT_STRUCT(owl::GetContainerFilesEvent, container_id, user_id);

#endif // OWL_VFS_MQ_SCHEMAS_EVENTS