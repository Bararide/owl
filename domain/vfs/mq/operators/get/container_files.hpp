#ifndef OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES
#define OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES

#include "vfs/core/handlers.hpp"

namespace owl {

template <typename EventSchema>
class GetContainerFiles
    : public EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema> {
public:
  using Base = EventHandlerBase<GetContainerFiles<EventSchema>, EventSchema>;
  using Base::Base;

  void operator()(const EventSchema &event) {
    spdlog::critical("Обработчик работает");
    // здесь у нас есть доступ к:
    // - this->state_
    // - event.container_id
    // - event.user_id
    // Реальная логика:
    //   1) найти контейнер
    //   2) получить файлы
    //   3) отправить ответ и т.д.
  }
};

} // namespace owl

#endif // OWL_VFS_CORE_OPERATORS_GET_CONTAINER_FILES