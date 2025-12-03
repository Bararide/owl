#ifndef OWL_API_REQUESTS
#define OWL_API_REQUESTS

#include <boost/hana.hpp>
#include <string>
#include <utility>
#include <vector>

namespace owl::api::validate {

struct GetContainerMetrics {
  uint16_t memory_limit;
  uint16_t cpu_limit;
};

} // namespace owl::api::validate

BOOST_HANA_ADAPT_STRUCT(owl::api::validate::GetContainerMetrics, memory_limit,
                        cpu_limit);

#endif // OWL_API_REQUESTS