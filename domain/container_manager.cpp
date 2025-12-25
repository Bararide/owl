#include "container_manager.hpp"

namespace owl {

ContainerManager *ContainerManager::instance_ = nullptr;
std::mutex ContainerManager::mutex_;

bool ContainerManager::register_container(
    std::shared_ptr<IKnowledgeContainer> container) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  std::string container_id = container->get_id();
  if (containers_.find(container_id) != containers_.end())
    return false;
  containers_[container_id] = container;
  return true;
}

bool ContainerManager::register_ossec_container(
    std::shared_ptr<ossec::PidContainer> ossec_container) {

  auto adapter =
      std::make_shared<OssecContainerAdapter>(ossec_container, *embedder_);
  return register_container(adapter);
}

bool ContainerManager::unregister_container(const std::string &container_id) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  auto it = containers_.find(container_id);
  if (it != containers_.end()) {
    containers_.erase(it);
    return true;
  }
  return false;
}

bool ContainerManager::delete_container(const std::string &container_id) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  auto it = containers_.find(container_id);
  if (it != containers_.end()) {
    auto container = it->second;

    if (auto ossec_adapter =
            std::dynamic_pointer_cast<OssecContainerAdapter>(container)) {
      auto native_container = ossec_adapter->get_native_container();
      if (native_container && native_container->is_running()) {
        auto stop_result = native_container->stop();
        if (!stop_result.is_ok()) {
          spdlog::warn("Failed to stop container {}: {}", container_id,
                       stop_result.error().what());
        } else {
          spdlog::info("Container stopped successfully: {}", container_id);
        }
      }
    }

    containers_.erase(it);
    return true;
  }
  return false;
}

std::shared_ptr<IKnowledgeContainer>
ContainerManager::get_container(const std::string &container_id) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  auto it = containers_.find(container_id);
  return it != containers_.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<IKnowledgeContainer>>
ContainerManager::get_all_containers() {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  std::vector<std::shared_ptr<IKnowledgeContainer>> result;
  for (const auto &[id, container] : containers_)
    result.push_back(container);
  return result;
}

std::vector<std::shared_ptr<IKnowledgeContainer>>
ContainerManager::get_containers_by_owner(const std::string &owner) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  std::vector<std::shared_ptr<IKnowledgeContainer>> result;
  for (const auto &[id, container] : containers_) {
    if (container->get_owner() == owner)
      result.push_back(container);
  }
  return result;
}

std::vector<std::shared_ptr<IKnowledgeContainer>>
ContainerManager::get_available_containers() {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  std::vector<std::shared_ptr<IKnowledgeContainer>> result;
  for (const auto &[id, container] : containers_) {
    if (container->is_available())
      result.push_back(container);
  }
  return result;
}

std::vector<std::shared_ptr<IKnowledgeContainer>>
ContainerManager::find_containers_by_label(const std::string &key,
                                           const std::string &value) {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  std::vector<std::shared_ptr<IKnowledgeContainer>> result;
  for (const auto &[id, container] : containers_) {
    auto labels = container->get_labels();
    auto it = labels.find(key);
    if (it != labels.end()) {
      if (value.empty() || it->second == value)
        result.push_back(container);
    }
  }
  return result;
}

size_t ContainerManager::get_container_count() const {
  // std::lock_guard<std::mutex> lock(containers_mutex_);
  return containers_.size();
}

size_t ContainerManager::get_available_container_count() {
  return get_available_containers().size();
}

void ContainerManager::clear() {
  std::lock_guard<std::mutex> lock(containers_mutex_);
  containers_.clear();
}

} // namespace owl