#ifndef OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS_HELPER
#define OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS_HELPER

#include "vfs/core/container/base/file_system_container.hpp"
#include "vfs/core/container/base/identifiable_container.hpp"
#include "vfs/core/container/base/resource_manager_container.hpp"
#include "vfs/core/container/base/search_container.hpp"
#include "vfs/core/container/base/stateful_container.hpp"

namespace owl {

namespace fs = std::filesystem;

template <typename Derived> class OssecFsHelpersMixin {
protected:
  bool isRootVirtualPath(const std::string &virtual_path) const {
    return virtual_path.empty() || virtual_path == "/";
  }

  std::string normalizeVirtualPath(const std::string &virtual_path) const {
    if (isRootVirtualPath(virtual_path)) {
      return "";
    }
    if (!virtual_path.empty() && virtual_path.front() == '/') {
      return virtual_path.substr(1);
    }
    return virtual_path;
  }

  std::string
  normalizeVirtualPathAsRooted(const std::string &virtual_path) const {
    auto normalized = normalizeVirtualPath(virtual_path);
    return "/" + normalized;
  }

  fs::path makeFullPath(const std::string &virtual_path,
                        const fs::path &data_path) const {
    const auto real_path = normalizeVirtualPath(virtual_path);
    return data_path / real_path;
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS_HELPER