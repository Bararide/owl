#ifndef CONTAINER_OWNER_OPERATIONS_HPP
#define CONTAINER_OWNER_OPERATIONS_HPP

namespace owl {

template <typename Derived> class PossesionOperations {
public:
  std::string getId() const noexcept {
    return static_cast<const Derived *>(this)->getId();
  }

  std::string getOwner() const noexcept {
    return static_cast<const Derived *>(this)->getOwner();
  }

  std::string getNamespace() const noexcept {
    return static_cast<const Derived *>(this)->getNamespace();
  }

  std::string getDataPath() const noexcept {
    return static_cast<const Derived *>(this)->getDataPath();
  }

  std::vector<std::string> getCommands() const noexcept {
    return static_cast<const Derived *>(this)->getCommands();
  }

  std::map<std::string, std::string> getLabels() const noexcept {
    return static_cast<const Derived *>(this)->getLabels();
  }
};

} // namespace owl

#endif // CONTAINER_OWNER_OPERATIONS_HPP