#ifndef OWL_MQ_RESOLVERS_ID
#define OWL_MQ_RESOLVERS_ID

#include <boost/hana.hpp>
#include <boost/hana/accessors.hpp>
#include <boost/hana/functional.hpp>
#include <infrastructure/result.hpp>
#include <nlohmann/json.hpp>

#include "../validation/validator.hpp"

namespace owl {

class Id final : public Validator<Id> {
public:
  template <typename Schema, typename Accessor>
  bool operator()(const nlohmann::json &json, Accessor accessor,
                  Schema &obj) const {
    auto name = boost::hana::first(accessor);
    auto member_ptr = boost::hana::second(accessor);

    std::string field = boost::hana::to<char const *>(name);
    if (!json.contains(field)) {
      spdlog::error("Missing field {}", field);
      return false;
    }

    return Validator<Id>::validateValue(json[field], member_ptr(obj));
  }
};

} // namespace owl

#endif // OWL_MQ_RESOLVERS_ID