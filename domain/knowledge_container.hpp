#ifndef KNOWLEDGE_CONTAINER_HPP
#define KNOWLEDGE_CONTAINER_HPP

#include "operations/container.hpp"
#include "operations/file.hpp"
#include "operations/owner.hpp"
#include "operations/search.hpp"

namespace owl::vectorfs {

template <typename Derived>
class KnowledgeContainer : public FileOperations<Derived>,
                           public SearchOperations<Derived>,
                           public ContainerManagement<Derived>,
                           public PossesionOperations<Derived> {

public:
  using FileOperationsBase = FileOperations<Derived>;
  using SearchOperationsBase = SearchOperations<Derived>;
  using PossesionOperationsBase = PossesionOperations<Derived>;
  using ContainerManagementBase = ContainerManagement<Derived>;

  virtual ~KnowledgeContainer() = default;
};

} // namespace owl::vectorfs

#endif