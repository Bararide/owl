#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <fuse3/fuse.h>
#include <utility>

#include "state.hpp"

namespace owl::vectorfs {

template <typename Derived> class BaseHandler {
protected:
  VectorFSState &state_;

public:
  explicit BaseHandler(VectorFSState &state) : state_(state) {}
  virtual ~BaseHandler() = default;

  template <typename... Args> int operator()(Args... args) {
    return static_cast<Derived *>(this)->handle(std::forward<Args>(args)...);
  }
};

} // namespace owl::vectorfs

#endif // HANDLER_HPP