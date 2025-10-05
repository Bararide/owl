#ifndef OWL_IPC_HANDLER_BASE
#define OWL_IPC_HANDLER_BASE

#include "ipc_base.hpp"
#include <pipeline/pipeline.hpp>

namespace owl {

template <typename Derived>
class IpcHandlerBase
    : public core::pipeline::PipelineHandler<Derived, schemas::FileInfo> {
public:
  core::Result<schemas::FileInfo> handle(schemas::FileInfo &file) {
    return static_cast<Derived *>(this)->handle(file);
  }

  void await() {
    if constexpr (requires(Derived d) { d.await(); }) {
      static_cast<Derived *>(this)->await();
    }
  }

  virtual ~IpcHandlerBase() = default;
};

} // namespace owl

#endif // OWL_IPC_HANDLER_BASE