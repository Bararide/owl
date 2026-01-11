#ifndef OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_STATE
#define OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_STATE

#include <infrastructure/result.hpp>
#include <string>

namespace owl {

template <typename Derived>
class OssecStateMixin : public StatefulContainer<Derived> {
public:
  using Error = std::runtime_error;

  std::string getStatus() const {
    auto native = derived().getNative();
    if (!native) {
      return "invalid";
    }

    const bool native_running = native->is_running();
    if (native_running) {
      return "running";
    }
    if (native->is_owned()) {
      return "stopped";
    }
    return "unknown";
  }

  bool isAvailable() const {
    auto native = derived().getNative();
    return native && native->is_running();
  }

  core::Result<void> ensureRunning() {
    auto native = derived().getNative();
    if (!native) {
      return core::Result<void, Error>::Error(Error("native is null"));
    }
    if (!native->is_running()) {
      auto r = native->start();
      if (!r.is_ok()) {
        return core::Result<void, Error>::Error(
            Error("start failed: " + std::string(r.error().what())));
      }
    }
    return core::Result<void, Error>::Ok();
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_STATE