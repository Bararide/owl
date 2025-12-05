#ifndef CONTAINER_MANAGEMENT_HPP
#define CONTAINER_MANAGEMENT_HPP

#include <map>
#include <string>
#include <vector>

namespace owl {

template <typename Derived> class ContainerManagement {
public:
  bool isAvailable() const {
    return static_cast<const Derived *>(this)->isAvailable();
  }

  size_t getSize() const {
    return static_cast<const Derived *>(this)->getSize();
  }

  std::string getStatus() const {
    return static_cast<const Derived *>(this)->getStatus();
  }

  bool updateAllEmbeddings() {
    return static_cast<Derived *>(this)->updateAllEmbeddings();
  }
};

} // namespace owl

#endif // CONTAINER_MANAGEMENT_HPP