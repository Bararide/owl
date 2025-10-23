#ifndef OWL_API_VALIDATE
#define OWL_API_VALIDATE

#include "bodies.hpp"
#include <boost/hana.hpp>
#include <infrastructure/result.hpp>
#include <json/value.h>
#include <spdlog/spdlog.h>

namespace owl::api::validate {

class Validator {
public:
  template <typename S>
  static inline core::Result<std::pair<std::string, int>, std::string>
  validate(const Json::Value &body) {
    S obj;
    bool success = hana::unpack(hana::accessors<S>(), [&](auto &&...accessor) {
      return (validateField(body, accessor, obj) && ...);
    });

    if (success) {
      return core::Result<std::pair<std::string, int>, std::string>::Ok(
          std::make_pair(obj.query, obj.limit));
    } else {
      return core::Result<std::pair<std::string, int>, std::string>::Error(
          "Validation failed");
    }
  }

private:
  template <typename S, typename Accessor>
  static bool validateField(const Json::Value &body, Accessor accessor,
                            S &obj) {
    auto name = hana::first(accessor);
    auto member_ptr = hana::second(accessor);
    std::string field_name = hana::to<char const *>(name);

    if (!body.isMember(field_name)) {
      spdlog::error("Missing field: {}", field_name);
      return false;
    }

    return validateValue(body[field_name], member_ptr(obj));
  }

  template <typename U>
  static bool validateValue(const Json::Value &json_value, U &member_ref) {
    if constexpr (std::is_same_v<U, std::string>) {
      if (json_value.isString()) {
        member_ref = json_value.asString();
        return true;
      }
    } else if constexpr (std::is_same_v<U, int>) {
      if (json_value.isInt()) {
        member_ref = json_value.asInt();
        return true;
      }
    } else if constexpr (std::is_same_v<U, bool>) {
      if (json_value.isBool()) {
        member_ref = json_value.asBool();
        return true;
      }
    } else if constexpr (std::is_same_v<U, double>) {
      if (json_value.isDouble()) {
        member_ref = json_value.asDouble();
        return true;
      }
    }

    spdlog::error("Type mismatch for value, expected: {}", typeid(U).name());
    return false;
  }
};

} // namespace owl::api::validate

#endif // OWL_API_VALIDATE