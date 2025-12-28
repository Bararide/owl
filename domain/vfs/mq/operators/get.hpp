#ifndef OWL_MQ_OPERATORS_VERBS
#define OWL_MQ_OPERATORS_VERBS

#include <nlohmann/json.hpp>

namespace owl {

template <typename Derived> struct GetVerb {
  template <typename... Args> auto get(Args &&...args) -> decltype(auto) {
    return static_cast<Derived *>(this)->handleGet(std::forward<Args>(args)...);
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_VERBS