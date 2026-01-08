#ifndef OWL_VFS_CORE_OPERATORS_CONTAINER_STOP
#define OWL_VFS_CORE_OPERATORS_CONTAINER_STOP

#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct ContainerStop final
    : FullContainerHandler<ContainerStop<EventSchema>, EventSchema> {
  using Base = FullContainerHandler<ContainerStop<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const auto &e) {
    this->process(e, [this](auto &, auto &, auto c) {
      return core::Result<bool>::Ok(true);
    });
  }

private:
  void onSuccess(bool result) {
    spdlog::info("Stop container success: {}", result);
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_CONTAINER_STOP