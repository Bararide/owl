#ifndef OWL_VFS_CORE_CONTAINER_STATEFUL_CONTAINER
#define OWL_VFS_CORE_CONTAINER_STATEFUL_CONTAINER

#include <infrastructure/result.hpp>

namespace owl {

template <typename Derived> class StatefulContainer {
public:
  std::string getStatus() const { return derived().getStatus(); }

  bool isAvailable() const { return derived().isAvailable(); }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_STATEFUL_CONTAINER