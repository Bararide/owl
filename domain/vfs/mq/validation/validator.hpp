#ifndef OWL_MQ_VALIDATION_VALIDATOR
#define OWL_MQ_VALIDATION_VALIDATOR

#include <boost/hana.hpp>
#include <infrastructure/result.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace owl {

template <typename Derived> class Validator {
public:
  template <typename Schema> static auto validate(const nlohmann::json &json) {
    Schema obj;
    bool success = boost::hana::unpack(
        boost::hana::accessors<Schema>(), [&](auto &&...accessor) {
          return (validateField(json, accessor, obj) && ...);
        });

    if (success) {
      return core::Result<Schema, std::string>::Ok(obj);
    }

    return core::Result<Schema, std::string>::Error("Validation failed");
  }

private:
  friend Derived;

  template <typename Schema, typename Accessor>
  static bool validateField(const nlohmann::json &json, Accessor accessor,
                            Schema &obj) {
    return Derived()(json, accessor, obj);
  }

  template <typename U>
  static bool validateValue(const nlohmann::json &json_value, U &member_ref) {
    if constexpr (std::is_same_v<U, std::string>) {
      if (json_value.is_string()) {
        member_ref = json_value.get<std::string>();
        return true;
      }
    } else if constexpr (std::is_same_v<U, int>) {
      if (json_value.is_number_integer()) {
        member_ref = json_value.get<int>();
        return true;
      }
    } else if constexpr (std::is_same_v<U, bool>) {
      if (json_value.is_boolean()) {
        member_ref = json_value.get<bool>();
        return true;
      }
    } else if constexpr (std::is_same_v<U, double>) {
      if (json_value.is_number_float()) {
        member_ref = json_value.get<double>();
        return true;
      }
    } else if constexpr (std::is_same_v<U, size_t>) {
      if (json_value.is_number_unsigned()) {
        member_ref = json_value.get<size_t>();
        return true;
      }
    } else if constexpr (std::is_same_v<U, std::vector<std::string>>) {
      if (json_value.is_array()) {
        try {
          member_ref = json_value.get<std::vector<std::string>>();
          return true;
        } catch (const nlohmann::json::exception &) {
          return false;
        }
      }
    } else if constexpr (std::is_same_v<U,
                                        std::pair<std::string, std::string>>) {
      if (json_value.is_object() && json_value.contains("key") &&
          json_value.contains("value")) {
        try {
          member_ref.first = json_value["key"].get<std::string>();
          member_ref.second = json_value["value"].get<std::string>();
          return true;
        } catch (const nlohmann::json::exception &) {
          return false;
        }
      }
    }

    spdlog::error("Type mismatch for value, expected: {}", typeid(U).name());
    return false;
  }
};

} // namespace owl

#endif // OWL_MQ_VALIDATION_VALIDATOR