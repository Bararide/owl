#ifndef OWL_VFS_CORE_CONTAINER_MANAGER_HPP
#define OWL_VFS_CORE_CONTAINER_MANAGER_HPP

#include "container_base.hpp"
#include <infrastructure/result.hpp>

#include <map>

namespace owl {

template <typename ContainerT> class ContainerManager {
public:
  using ContainerPtr = std::shared_ptr<ContainerT>;
  using Error = std::runtime_error;

  ContainerManager() = default;

  ContainerManager(const ContainerManager &) = delete;
  ContainerManager &operator=(const ContainerManager &) = delete;

  core::Result<void> registerContainer(ContainerPtr container) {
    if (!container) {
      return core::Result<void, Error>::Error(Error("null container"));
    }
    std::lock_guard lock(mutex_);
    const auto id = container->getId();
    auto [it, inserted] = containers_.emplace(id, std::move(container));
    if (!inserted) {
      return core::Result<void, Error>::Error(
          Error("container already registered: " + id));
    }
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> unregisterContainer(const std::string &id) {
    std::lock_guard lock(mutex_);
    auto it = containers_.find(id);
    if (it == containers_.end()) {
      return core::Result<void, Error>::Error(
          Error("no such container: " + id));
    }
    containers_.erase(it);
    return core::Result<void, Error>::Ok();
  }

  core::Result<ContainerPtr> getContainer(const std::string &id) const {
    std::lock_guard lock(mutex_);
    auto it = containers_.find(id);
    if (it == containers_.end()) {
      return core::Result<ContainerPtr, Error>::Error(
          Error("no such container: " + id));
    }
    return core::Result<ContainerPtr, Error>::Ok(it->second);
  }

  core::Result<void> deleteContainer(const std::string &id) {
    return unregisterContainer(id);
  }

  std::vector<ContainerPtr> getAllContainers() const {
    std::lock_guard lock(mutex_);
    std::vector<ContainerPtr> out;
    out.reserve(containers_.size());
    for (const auto &kv : containers_) {
      out.push_back(kv.second);
    }
    return out;
  }

  std::vector<ContainerPtr>
  getContainersByOwner(const std::string &owner) const {
    std::lock_guard lock(mutex_);
    std::vector<ContainerPtr> out;
    for (const auto &kv : containers_) {
      if (kv.second->getOwner() == owner) {
        out.push_back(kv.second);
      }
    }
    return out;
  }

  std::vector<ContainerPtr> getAvailableContainers() const {
    std::lock_guard lock(mutex_);
    std::vector<ContainerPtr> out;
    for (const auto &kv : containers_) {
      if (kv.second->isAvailable()) {
        out.push_back(kv.second);
      }
    }
    return out;
  }

  std::vector<ContainerPtr>
  findContainersByLabel(const std::string &key,
                        const std::string &value = "") const {
    std::lock_guard lock(mutex_);
    std::vector<ContainerPtr> out;
    for (const auto &kv : containers_) {
      auto labels = kv.second->getLabels();
      auto it = labels.find(key);
      if (it == labels.end())
        continue;
      if (value.empty() || it->second == value) {
        out.push_back(kv.second);
      }
    }
    return out;
  }

  std::vector<std::string> getCommands() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> cmds;
    for (const auto &kv : containers_) {
      auto c = kv.second->getCommands();
      cmds.insert(cmds.end(), c.begin(), c.end());
    }
    return cmds;
  }

  std::size_t getContainerCount() const {
    std::lock_guard lock(mutex_);
    return containers_.size();
  }

  std::size_t getAvailableContainerCount() const {
    std::lock_guard lock(mutex_);
    std::size_t count = 0;
    for (const auto &kv : containers_) {
      if (kv.second->isAvailable())
        ++count;
    }
    return count;
  }

  void clear() {
    std::lock_guard lock(mutex_);
    containers_.clear();
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, ContainerPtr> containers_;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MANAGER_HPP