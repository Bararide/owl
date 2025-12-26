#ifndef OWL_MQ_OPERATORS_POST
#define OWL_MQ_OPERATORS_POST

#include <nlohmann/json.hpp>

namespace owl {

template <typename Derived> class Post {
public:
  template <typename... Args> auto handle(Args &&...args) -> decltype(auto) {
    return static_cast<Derived *>(this)->handle(std::forward<Args>(args)...);
  }

private:
  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_POST