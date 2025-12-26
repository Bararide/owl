#ifndef OWL_MQ_OPERATORS_GET
#define OWL_MQ_OPERATORS_GET

#include <nlohmann/json.hpp>

namespace owl {

template <typename Derived> class Get {
public:
  template <typename... Args> auto handle(Args &&...args) -> decltype(auto) {
    return static_cast<Derived *>(this)->handle(std::forward<Args>(args)...);
  }

private:
  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_GET