#ifndef OWL_VFS_CORE_OPERATORS_CREATE_CONTAINER
#define OWL_VFS_CORE_OPERATORS_CREATE_CONTAINER

#include "vfs/mq/operators/resolvers/resolvers.hpp"

namespace owl {

template <typename EventSchema>
struct CreateContainer final
    : CreateContainerHandler<CreateContainer<EventSchema>, EventSchema> {
  using Base =
      CreateContainerHandler<CreateContainer<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &e) {
    this->process(e, [this](auto &s, auto &ev, bool not_exist) {
      if (not_exist) {
        return core::Result<std::shared_ptr<OssecContainer<>>>::Ok(
            {} // s.container_manager_.createContainer(ev.container_id)
        );
      }

      return core::Result<std::shared_ptr<OssecContainer<>>>::Error(
          std::runtime_error("Container exists"));
    });
  }

private:
  void onSuccess(std::shared_ptr<OssecContainer<>> container) {
    spdlog::info("Create success");
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_CREATE_CONTAINER