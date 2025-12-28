#ifndef OWL_MQ_RESOLVERS_ID
#define OWL_MQ_RESOLVERS_ID

#include "vfs/mq/validation/validator.hpp"

namespace owl {

struct ByIdValidator : Validator<ByIdValidator> {
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

    return validateValue(json[field], member_ptr(obj));
  }
};

} // namespace owl

#endif // OWL_MQ_RESOLVERS_ID