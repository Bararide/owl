#ifndef OWL_MQ_CONTROLLERS_CONTROLLERS
#define OWL_MQ_CONTROLLERS_CONTROLLERS

#include "container_create.hpp"
#include "container_delete.hpp"
#include "container_get_files.hpp"
#include "container_stop.hpp"
#include "file_create.hpp"
#include "file_delete.hpp"
#include "semantic_search.hpp"

#include "vfs/mq/core/dispatcher.hpp"
#include "vfs/mq/core/routing.hpp"

#include "vfs/mq/schemas/events.hpp"
#include "vfs/mq/schemas/schemas.hpp"

namespace owl {

using ContainerCreateRoute = Route<Verb::Post, ContainerCreateSchema, Path<container_sv, create_sv>, ContainerCreateController>;
using GetContainerFilesRoute = Route<Verb::Get, ContainerGetFilesSchema, Path<container_sv, files_sv>, ContainerGetFilesController>;
using ContainerDeleteRoute = Route<Verb::Delete, ContainerDeleteSchema, Path<container_sv, delete_sv>, ContainerDeleteController>;
using FileCreateRoute = Route<Verb::Post, FileCreateSchema, Path<file_sv, create_sv>, FileCreateController>;
using FileDeleteRoute = Route<Verb::Delete, FileDeleteSchema, Path<file_sv, delete_sv>, FileDeleteController>;
using ContainerStopRoute = Route<Verb::Post, ContainerStopSchema, Path<container_sv, stop_sv>, ContainerStopController>;
using SemanticSearchRoute = Route<Verb::Post, SemanticSearchSchema, Path<search_sv, semantic_sv>, SemanticSearchController>;

using MQDispatcher = Dispatcher<ContainerCreateRoute, GetContainerFilesRoute, ContainerDeleteRoute, FileCreateRoute, FileDeleteRoute, ContainerStopRoute, SemanticSearchRoute>;

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTROLLERS