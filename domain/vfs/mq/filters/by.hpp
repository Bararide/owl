#ifndef OWL_MQ_RESOLVERS_BY
#define OWL_MQ_RESOLVERS_BY

#include "../resolvers/id.hpp"
#include "../validation/validator.hpp"

namespace owl {

template <typename Derived> struct By : public Validator<By<Derived>> {
  template <typename... Args> auto handle(Args &&...args) -> decltype(auto) {
    return Derived::handle(std::forward<Args>(args)...);
  }
};

} // namespace owl

#endif // OWL_MQ_RESOLVERS_BY