#ifndef OWL_VFS_CORE_CONTAINER_MANAGER_HPP
#define OWL_VFS_CORE_CONTAINER_MANAGER_HPP

#include <infrastructure/result.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace owl {

template <typename ContainerT> class ContainerManager {
public:
  using ContainerPtr = std::shared_ptr<ContainerT>;
  using ContainerMap = std::map<std::string, ContainerPtr>;
  using ContainerList = std::vector<ContainerPtr>;
  using Error = std::runtime_error;

  ContainerManager() = default;

  ContainerManager(const ContainerManager &) = delete;
  ContainerManager &operator=(const ContainerManager &) = delete;
  ContainerManager(ContainerManager &&) = default;
  ContainerManager &operator=(ContainerManager &&) = default;

  template <typename... Args>
  core::Result<void> createAndRegisterContainer(Args &&...args) {
    std::lock_guard lock(mutex_);

    auto container = std::make_shared<ContainerT>(std::forward<Args>(args)...);

    if (!validateContainer(container)) {
      return core::Result<void, Error>::Error(Error("Invalid container"));
    }

    const auto &id = container->getId();

    if (containers_.find(id) != containers_.end()) {
      return core::Result<void, Error>::Error(
          Error("Container already registered: " + id));
    }

    containers_.emplace(id, std::move(container));
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> registerContainer(ContainerPtr container) {
    if (!validateContainer(container)) {
      return core::Result<void, Error>::Error(Error("Invalid container"));
    }

    std::lock_guard lock(mutex_);
    const auto &id = container->getId();

    if (containers_.find(id) != containers_.end()) {
      return core::Result<void, Error>::Error(
          Error("Container already registered: " + id));
    }

    containers_.emplace(id, std::move(container));
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> unregisterContainer(const std::string &id) {
    if (!validateContainerId(id)) {
      return core::Result<void, Error>::Error(Error("Invalid container ID"));
    }

    std::lock_guard lock(mutex_);

    if (!removeContainerUnsafe(id)) {
      return core::Result<void, Error>::Error(
          Error("No such container: " + id));
    }

    return core::Result<void, Error>::Ok();
  }

  core::Result<ContainerPtr> getContainer(const std::string &id) const {
    if (!validateContainerId(id)) {
      return core::Result<ContainerPtr, Error>::Error(
          Error("Invalid container ID"));
    }

    std::lock_guard lock(mutex_);
    auto container = findContainerUnsafe(id);

    if (!container) {
      return core::Result<ContainerPtr, Error>::Error(
          Error("No such container: " + id));
    }

    return core::Result<ContainerPtr, Error>::Ok(container);
  }

  core::Result<void> deleteContainer(const std::string &id) {
    return unregisterContainer(id);
  }

  ContainerList getAllContainers() const {
    std::lock_guard lock(mutex_);
    return filterContainersUnsafe([](const ContainerPtr &) { return true; });
  }

  ContainerList getContainersByOwner(const std::string &owner) const {
    std::lock_guard lock(mutex_);
    return filterContainersUnsafe([&owner](const ContainerPtr &container) {
      return container->getOwner() == owner;
    });
  }

  ContainerList getAvailableContainers() const {
    std::lock_guard lock(mutex_);
    return filterContainersUnsafe(
        [](const ContainerPtr &container) { return container->isAvailable(); });
  }

  ContainerList findContainersByLabel(const std::string &key,
                                      const std::string &value = "") const {
    std::lock_guard lock(mutex_);
    return filterContainersUnsafe(
        [&key, &value](const ContainerPtr &container) {
          auto labels = container->getLabels();
          auto it = labels.find(key);

          if (it == labels.end()) {
            return false;
          }

          return value.empty() || it->second == value;
        });
  }

  std::vector<std::string> getCommands() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;
    for (const auto &[id, container] : containers_) {
      auto commands = container->getCommands();
      result.insert(result.end(), commands.begin(), commands.end());
    }
    return result;
  }

  std::size_t getContainerCount() const {
    std::lock_guard lock(mutex_);
    return containers_.size();
  }

  std::size_t getAvailableContainerCount() const {
    std::lock_guard lock(mutex_);
    return std::count_if(
        containers_.begin(), containers_.end(),
        [](const auto &pair) { return pair.second->isAvailable(); });
  }

  void clear() {
    std::lock_guard lock(mutex_);
    containers_.clear();
  }

  bool contains(const std::string &id) const {
    if (!validateContainerId(id)) {
      return false;
    }

    std::lock_guard lock(mutex_);
    return containers_.find(id) != containers_.end();
  }

  bool isEmpty() const {
    std::lock_guard lock(mutex_);
    return containers_.empty();
  }

private:
  bool validateContainer(const ContainerPtr &container) const {
    return container != nullptr && !container->getId().empty();
  }

  bool validateContainerId(const std::string &id) const { return !id.empty(); }

  ContainerPtr findContainerUnsafe(const std::string &id) const {
    auto it = containers_.find(id);
    return it != containers_.end() ? it->second : nullptr;
  }

  bool removeContainerUnsafe(const std::string &id) {
    return containers_.erase(id) > 0;
  }

  template <typename Predicate>
  ContainerList filterContainersUnsafe(Predicate predicate) const {
    ContainerList result;
    for (const auto &[id, container] : containers_) {
      if (predicate(container)) {
        result.push_back(container);
      }
    }
    return result;
  }

  mutable std::mutex mutex_;
  ContainerMap containers_;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MANAGER_HPP