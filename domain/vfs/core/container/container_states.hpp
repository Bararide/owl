#ifndef OWL_VFS_CORE_CONTAINER_STATE_HPP
#define OWL_VFS_CORE_CONTAINER_STATE_HPP

#include <infrastructure/fsm/fsm.hpp>
#include <infrastructure/result.hpp>

namespace owl::container {

struct Stopped {};
struct Running {};
struct Invalid {};
struct Unknown {};

} // namespace owl::container

namespace core {

template <> struct is_state<owl::container::Stopped> : std::true_type {};
template <> struct is_state<owl::container::Running> : std::true_type {};
template <> struct is_state<owl::container::Invalid> : std::true_type {};
template <> struct is_state<owl::container::Unknown> : std::true_type {};

} // namespace core

namespace owl {

using StateVariant = std::variant<container::Stopped, container::Running,
                                  container::Invalid, container::Unknown>;

using ContainerTransitionTable = core::TransitionTable<
    core::Transition<container::Stopped, container::Running>,
    core::Transition<container::Running, container::Stopped>,
    core::Transition<container::Unknown, container::Running>,
    core::Transition<container::Unknown, container::Stopped>,
    core::Transition<container::Invalid, container::Stopped>>;

using ContainerStateMachine =
    core::StateMachine<StateVariant, ContainerTransitionTable>;

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_STATE_HPP