#ifndef OWL_VFS_CORE_CONTAINER_IDENTIFIABLE_CONTAINER
#define OWL_VFS_CORE_CONTAINER_IDENTIFIABLE_CONTAINER

#include <map>

namespace owl {

template <typename Derived> class IdentifiableContainer {
public:
  std::string getId() const { return derived().getId(); }

  std::string getOwner() const { return derived().getOwner(); }

  std::string getNamespace() const { return derived().getNamespace(); }

  std::string getDataPath() const { return derived().getDataPath(); }

  std::vector<std::string> getCommands() const {
    return derived().getCommands();
  }

  std::map<std::string, std::string> getLabels() const {
    return derived().getLabels();
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_IDENTIFIABLE_CONTAINER