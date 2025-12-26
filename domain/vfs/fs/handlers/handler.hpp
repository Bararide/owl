#ifndef OWL_VFS_FS_HANDLER
#define OWL_VFS_FS_HANDLER

#include "vfs/domain.hpp"

namespace owl {

template <typename Derived> class Handler {
protected:
  State &state_;

public:
  Handler() = delete;
  ~Handler() = default;

  explicit Handler(State &state) : state_(state) {}

  template <typename... Args> auto operator()(Args &&...args) {
    if constexpr (std::is_invocable_v<Derived &, Args...>) {
      return static_cast<Derived &>(*this)(std::forward<Args>(args)...);
    } else if constexpr (std::is_invocable_v<const Derived &, Args...>) {
      return static_cast<const Derived &>(*this)(std::forward<Args>(args)...);
    } else {
      static_assert(std::is_invocable_v<Derived &, Args...> ||
                        std::is_invocable_v<const Derived &, Args...>,
                    "Derived class must have appropriate operator()");
    }
  }

  template <typename... Args> auto operator()(Args &&...args) const {
    static_assert(std::is_invocable_v<const Derived &, Args...>,
                  "Derived class must have const operator()");

    return static_cast<const Derived &>(*this)(std::forward<Args>(args)...);
  }

  template <typename... Args> static int callback(Args &&...args) {
    struct fuse_context *ctx = fuse_get_context();

    if (ctx && ctx->private_data) {
      Derived *handler = static_cast<Derived *>(ctx->private_data);
      return (*handler)(std::forward<Args>(args)...);
    }

    spdlog::warn(
        "No private_data in fuse_context, using default implementation");

    return -ENOSYS;
  }
};

} // namespace owl

#endif // OWL_VFS_FS_HANDLER