#ifndef OWL_MQ_SCHEMAS
#define OWL_MQ_SCHEMAS

#include <boost/hana.hpp>
#include <string>
#include <utility>
#include <vector>

namespace owl {

struct Container {
  std::string container_id;

  BOOST_HANA_DEFINE_STRUCT(Container, (std::string, container_id));
};

struct User {
  std::string user_id;

  BOOST_HANA_DEFINE_STRUCT(Container, (std::string, user_id));
};

} // namespace owl

#endif // OWL_MQ_SCHEMAS