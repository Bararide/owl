#ifndef GIMBAL_TR_OS_DOMAIN_FSM_STATES
#define GIMBAL_TR_OS_DOMAIN_FSM_STATES

#include <boost/fusion/adapted.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/hana.hpp>
#include <boost/preprocessor.hpp>
#include <type_traits>

namespace gimbal::tr_os {

enum class CameraStates {
    IrN,
    TvN,
    IrW,
    TvW
};

enum class TrackerStates {
    run,
    init,
    stop,
    lost
};

struct CameraState {
    CameraStates state_;
};

struct TrackerState {
    TrackerStates state_;
};

}  // namespace gimbal::tr_os

BOOST_FUSION_ADAPT_STRUCT(gimbal::tr_os::CameraState, state_)
BOOST_FUSION_ADAPT_STRUCT(gimbal::tr_os::TrackerState, state_)

#endif  // GIMBAL_TR_OS_DOMAIN_FSM_STATES