#ifndef OWL_MQ_SCHEMAS
#define OWL_MQ_SCHEMAS

#include <boost/hana.hpp>
#include <string>

namespace owl {

struct ContainerSchema {
  std::string container_id;
};

struct UserSchema {
  std::string user_id;
};

} // namespace owl

BOOST_HANA_ADAPT_STRUCT(owl::ContainerSchema, container_id);
BOOST_HANA_ADAPT_STRUCT(owl::UserSchema, user_id);

#endif // OWL_MQ_SCHEMAS