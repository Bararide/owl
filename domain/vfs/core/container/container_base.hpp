#ifndef OWL_VFS_CORE_CONTAINER_CONTAINER_BASE
#define OWL_VFS_CORE_CONTAINER_CONTAINER_BASE

#include "vfs/core/container/base/file_system_container.hpp"
#include "vfs/core/container/base/identifiable_container.hpp"
#include "vfs/core/container/base/resource_manager_container.hpp"
#include "vfs/core/container/base/search_container.hpp"
#include "vfs/core/container/base/stateful_container.hpp"

#include "vfs/core/schemas/filesystem_schemas.hpp"

namespace owl {

template <typename Derived>
class ContainerBase : public IdentifiableContainer<Derived>, public FileSystemContainer<Derived>, public ResourceManagedContainer<Derived>, public SearchableContainer<Derived>, public StatefulContainer<Derived> {
public:
  using Self = Derived;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_CONTAINER_BASE