#ifndef OWL_VFS_MQ_SCHEMAS_EVENTS
#define OWL_VFS_MQ_SCHEMAS_EVENTS

#include <boost/fusion/functional.hpp>
#include <boost/hana.hpp>

namespace owl {

struct GetContainerFilesEvent {
  std::string container_id;
  std::string user_id;
};

} // namespace owl

BOOST_HANA_ADAPT_STRUCT(owl::GetContainerFilesEvent, container_id, user_id);

#endif // OWL_VFS_MQ_SCHEMAS_EVENTS