#ifndef OWL_MQ_OPERATORS_EVENT_HANDLERS
#define OWL_MQ_OPERATORS_EVENT_HANDLERS

#include "vfs/mq/operators/get/container_files.hpp"
#include "vfs/mq/schemas/events.hpp"

namespace owl {

using Operators = EventHandlers<GetContainerFiles<GetContainerFilesEvent>,
                                GetContainerFiles<SemanticSearchEvent>>;

}

#endif // OWL_MQ_OPERATORS_EVENT_HANDLERS