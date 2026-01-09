#ifndef OWL_MQ_CONTROLLERS_CONTROLLERS
#define OWL_MQ_CONTROLLERS_CONTROLLERS

#include "container_create.hpp"
#include "container_delete.hpp"
#include "get_container_files.hpp"
#include "container_stop.hpp"
#include "file_create.hpp"
#include "file_delete.hpp"
#include "semantic_search.hpp"

#include "vfs/mq/core/dispatcher.hpp"
#include "vfs/mq/core/routing.hpp"

#include "vfs/core/schemas/events.hpp"
#include "vfs/core/schemas/schemas.hpp"

namespace owl {

using ContainerCreateRoute = Route<Verb::Post, ContainerCreateSchema, ContainerCreateEvent, Path<container_sv, create_sv>, Controller<ContainerCreateController>>;
using GetContainerFilesRoute = Route<Verb::Get, ContainerGetFilesSchema, GetContainerFilesEvent, Path<container_sv, files_sv>, Controller<ContainerGetFilesController>>;
using ContainerDeleteRoute = Route<Verb::Delete, ContainerDeleteSchema, ContainerDeleteEvent, Path<container_sv, delete_sv>, Controller<ContainerDeleteController>>;
using FileCreateRoute = Route<Verb::Post, FileCreateSchema, FileCreateEvent, Path<file_sv, create_sv>, Controller<FileCreateController>>;
using FileDeleteRoute = Route<Verb::Delete, FileDeleteSchema, FileDeleteEvent, Path<file_sv, delete_sv>, Controller<FileDeleteController>>;
using ContainerStopRoute = Route<Verb::Post, ContainerStopSchema, ContainerStopEvent, Path<container_sv, stop_sv>, Controller<ContainerStopController>>;
using SemanticSearchRoute = Route<Verb::Post, SemanticSearchSchema, SemanticSearchEvent, Path<search_sv, semantic_sv>, Controller<SemanticSearchController>>;

using MQDispatcher = Dispatcher<ContainerCreateRoute, GetContainerFilesRoute, ContainerDeleteRoute, FileCreateRoute, FileDeleteRoute, ContainerStopRoute, SemanticSearchRoute>;

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTROLLERS