#ifndef KNOWLEDGE_CONTAINER_HPP
#define KNOWLEDGE_CONTAINER_HPP

#include "container.hpp"
#include "file.hpp"
#include "owner.hpp"
#include "search.hpp"

#include <filesystem>
#include <fstream>

namespace owl {

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

} // namespace owl

#endif