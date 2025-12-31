#ifndef CORE_FSM_STATES
#define CORE_FSM_STATES

#include <infrastructure/fsm/fsm.hpp>

namespace owl {

using core::is_state;

enum class CameraStates { IrN, TvN, IrW, TvW };

enum class TrackerStates { run, init, stop, lost };

struct CameraState {
  CameraStates state_;
};

struct TrackerState {
  TrackerStates state_;
};

struct CameraFsm : core::StateBase<CameraFsm> {
  CameraState hw_state_;

  template <typename From, typename To>
  constexpr void doTransition(const From & /*from*/, const To & /*to*/) {
    if constexpr (std::is_same_v<From, CameraState> &&
                  std::is_same_v<To, CameraState>) {
      // Реальный код переключения
      // hw_switch(...);
    } else {
      static_assert(sizeof(From) == 0, "Unsupported transition");
    }
  }
};

} // namespace owl

namespace core {
  template <> struct is_state<owl::CameraState> : std::true_type {};
  template <> struct is_state<owl::TrackerState> : std::true_type {};
}


BOOST_FUSION_ADAPT_STRUCT(owl::CameraState, state_)
BOOST_FUSION_ADAPT_STRUCT(owl::TrackerState, state_)

#endif // CORE_FSM_STATES