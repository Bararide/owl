#ifndef OWL_MQ_OPERATORS_RESOLVERS_RESOLVERS
#define OWL_MQ_OPERATORS_RESOLVERS_RESOLVERS

#include "container/active.hpp"
#include "container/exists.hpp"
#include "container/ownership.hpp"

namespace owl {

template <typename State, typename Event>
using ContainerResolverChain =
    ResolverChain<State, Event, std::shared_ptr<IKnowledgeContainer>,
                  std::runtime_error, ContainerExists<State, Event>,
                  ContainerOwnership<State, Event>>;

template <typename State, typename Event>
using FullContainerResolverChain =
    ResolverChain<State, Event, std::shared_ptr<IKnowledgeContainer>,
                  std::runtime_error, ContainerExists<State, Event>,
                  ContainerOwnership<State, Event>,
                  ContainerIsActive<State, Event>>;

template <typename State, typename Event> auto createContainerResolverChain() {
  return ContainerResolverChain<State, Event>(
      ContainerExists<State, Event>{}, ContainerOwnership<State, Event>{});
}

template <typename State, typename Event>
auto createFullContainerResolverChain() {
  return FullContainerResolverChain<State, Event>(
      ContainerExists<State, Event>{}, ContainerOwnership<State, Event>{},
      ContainerIsActive<State, Event>{});
}

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_RESOLVERS