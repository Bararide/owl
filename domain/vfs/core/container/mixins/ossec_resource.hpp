#ifndef OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_RESOURCES
#define OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_RESOURCES

#include <sstream>
#include <string>

#include <infrastructure/result.hpp>
#include <spdlog/spdlog.h>

namespace owl {

template <typename Derived>
class OssecResourceMixin : public ResourceManagedContainer<Derived> {
public:
  using Error = std::runtime_error;

  core::Result<void> setResourceLimit(const std::string &resource_name,
                                      const std::string &value) {
    if (!derived().getNative()) {
      return core::Result<void, Error>::Error(
          Error("native container is null"));
    }

    try {
      if (resource_name == "memory") {
        return setMemoryLimit(value);
      }
      if (resource_name == "disk") {
        return setDiskLimit(value);
      }
      if (resource_name == "pids") {
        return setPidsLimit(value);
      }
      if (resource_name == "apply") {
        spdlog::info("Applying resource changes for container {}",
                     derived().getId());
        return core::Result<void, Error>::Ok();
      }

      return core::Result<void, Error>::Error(
          Error("unknown resource_name: " + resource_name));
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("setResourceLimit error: ") + e.what()));
    }
  }

  core::Result<std::string> getCurrentResources() const {
    if (!derived().getNative()) {
      return core::Result<std::string, Error>::Error(
          Error("native container is null"));
    }

    const auto &cont = derived().getNative()->get_container();
    std::stringstream ss;

    ss << "=== Current Resource Limits ===\n\n";
    ss << "Memory: " << cont.resources.memory_capacity << " bytes ";
    ss << "(" << cont.resources.memory_capacity / (1024 * 1024) << " MB)\n";

    ss << "Disk: " << cont.resources.storage_quota << " bytes ";
    ss << "(" << cont.resources.storage_quota / (1024 * 1024) << " MB)\n";

    ss << "Max Processes/Files: " << cont.resources.max_open_files << "\n";

    ss << "\nChange with: echo 'VALUE' > /containers/" << derived().getId()
       << "/.resources/RESOURCE_NAME\n";
    ss << "Apply changes: echo 'apply' > /containers/" << derived().getId()
       << "/.resources/apply\n";

    return core::Result<std::string, Error>::Ok(ss.str());
  }

private:
  core::Result<void> setMemoryLimit(const std::string &mb_value) {
    auto &cont = derived().getNative()->get_container();

    const std::size_t mb = std::stoull(mb_value);
    const std::size_t bytes = mb * 1024 * 1024;

    cont.resources.memory_capacity = bytes;

    if (!cont.cgroup_path.empty()) {
      ossec::PidResources::set_memory_limit(cont.cgroup_path, bytes);
    }
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> setDiskLimit(const std::string &mb_value) {
    auto &cont = derived().getNative()->get_container();

    const std::size_t mb = std::stoull(mb_value);
    const std::size_t bytes = mb * 1024 * 1024;

    cont.resources.storage_quota = bytes;
    return core::Result<void, Error>::Ok();
  }

  core::Result<void> setPidsLimit(const std::string &value) {
    auto &cont = derived().getNative()->get_container();

    const std::size_t max_pids = std::stoull(value);

    cont.resources.max_open_files = max_pids;
    if (!cont.cgroup_path.empty()) {
      ossec::PidResources::set_pids_limit(cont.cgroup_path, max_pids);
    }
    return core::Result<void, Error>::Ok();
  }

  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_RESOURCES