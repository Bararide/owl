#ifndef OWL_MQ_OPERATORS_EVENT_HANDLERS
#define OWL_MQ_OPERATORS_EVENT_HANDLERS

#include "vfs/mq/operators/container_stop.hpp"
#include "vfs/mq/operators/create_container.hpp"
#include "vfs/mq/operators/delete_container.hpp"
#include "vfs/mq/operators/file_create.hpp"
#include "vfs/mq/operators/file_delete.hpp"
#include "vfs/mq/operators/get_container_files.hpp"
#include "vfs/core/schemas/events.hpp"

namespace owl {

using Operators =
    EventHandlers<GetContainerFiles<GetContainerFilesEvent>,
                  GetContainerFiles<SemanticSearchEvent>,
                  CreateContainer<ContainerCreateEvent>,
                  DeleteContainer<ContainerDeleteEvent>,
                  FileCreate<FileCreateEvent>, 
                  FileDelete<FileDeleteEvent>,
                  ContainerStop<ContainerStopEvent>>;
                  
}

#endif // OWL_MQ_OPERATORS_EVENT_HANDLERS