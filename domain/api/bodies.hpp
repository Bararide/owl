#ifndef OWL_API_BODIES
#define OWL_API_BODIES

#include <boost/hana.hpp>

namespace owl::api::validate {

namespace hana = boost::hana;

struct SemanticSearch {
  std::string query;
  int limit;
};

} // namespace owl::api::validate

BOOST_HANA_ADAPT_STRUCT(owl::api::validate::SemanticSearch, query, limit);

#endif // OWL_API_BODIES