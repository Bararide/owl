#ifndef OWL_VFS_CORE_LOOP_RUNNER
#define OWL_VFS_CORE_LOOP_RUNNER

#include "loop_base.hpp"

namespace owl {

template <typename Derived, typename TLoop> class LoopRunner {
public:
  void start() { static_cast<Derived *>(this)->start(); }

  void stop() { static_cast<Derived *>(this)->stop(); }

  ~LoopRunner() = default;

};

template <class TLoopPtr, class TTimer>
void runLoop(TLoopPtr loop, TTimer *loop_timer) {
  loop->start();
  loop_timer->setStartTimePoint();
  while (loop->getIsActive()) {
    loop->update();
    loop_timer->nextIteration();
    loop_timer->waitRemainingIterationTime();
  }
  loop->stop();
}

} // namespace owl

#endif // OWL_VFS_CORE_LOOP_RUNNER