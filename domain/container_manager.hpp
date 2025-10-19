#ifndef VECTORFS_CONTAINER_MANAGER_HPP
#define VECTORFS_CONTAINER_MANAGER_HPP

#include "knowledge_container.hpp"
#include "ossec_container_adapter.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace owl::vectorfs {

class ContainerManager {
private:
  static ContainerManager *instance_;
  static std::mutex mutex_;

  std::map<std::string, std::shared_ptr<IKnowledgeContainer>> containers_;
  std::mutex containers_mutex_;
  chunkees::Search *search_ = nullptr;

public:
  ContainerManager(const ContainerManager &) = delete;
  ContainerManager &operator=(const ContainerManager &) = delete;

  static ContainerManager &get_instance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
      instance_ = new ContainerManager();
    }
    return *instance_;
  }

  ContainerManager() = default;

  static void destroy_instance() {
    std::lock_guard<std::mutex> lock(mutex_);
    delete instance_;
    instance_ = nullptr;
  }

  void set_search(chunkees::Search &search) { search_ = &search; }

  bool register_container(std::shared_ptr<IKnowledgeContainer> container);
  bool register_ossec_container(
      std::shared_ptr<ossec::PidContainer> ossec_container);
  bool unregister_container(const std::string &container_id);
  std::shared_ptr<IKnowledgeContainer>
  get_container(const std::string &container_id);
  std::vector<std::string> get_commands();
  std::vector<std::shared_ptr<IKnowledgeContainer>> get_all_containers();
  std::vector<std::shared_ptr<IKnowledgeContainer>>
  get_containers_by_owner(const std::string &owner);
  std::vector<std::shared_ptr<IKnowledgeContainer>> get_available_containers();
  std::vector<std::shared_ptr<IKnowledgeContainer>>
  find_containers_by_label(const std::string &key,
                           const std::string &value = "");
  size_t get_container_count() const;
  size_t get_available_container_count();
  void clear();
};

} // namespace owl::vectorfs

#endif