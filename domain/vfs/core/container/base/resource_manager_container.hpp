#ifndef OWL_VFS_CORE_CONTAINER_RESOURCE_MANAGER_CONTAINER
#define OWL_VFS_CORE_CONTAINER_RESOURCE_MANAGER_CONTAINER

#include <infrastructure/result.hpp>

namespace owl {

template <typename Derived> class ResourceManagedContainer {
public:
  core::Result<void> setResourceLimit(const std::string &resource_name,
                                      const std::string &value) {
    return derived().setResourceLimit(resource_name, value);
  }

  core::Result<std::string> getCurrentResources() const {
    return derived().getCurrentResources();
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_RESOURCE_MANAGER_CONTAINER