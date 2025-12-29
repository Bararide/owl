#ifndef OWL_MQ_ROUTING_DISPATCHER
#define OWL_MQ_ROUTING_DISPATCHER

#include "routing.hpp"
#include "vfs/mq/controller.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace owl {

inline std::vector<std::string_view> splitPath(std::string_view path) {
  std::vector<std::string_view> result;
  size_t start = 0;
  while (start < path.size()) {
    auto pos = path.find('/', start);
    if (pos == std::string_view::npos) {
      pos = path.size();
    }
    if (pos > start) {
      result.emplace_back(path.data() + start, pos - start);
    }
    start = pos + 1;
  }
  return result;
}

template <typename TPath>
inline bool matchPath(const std::vector<std::string_view> &segments) {
  constexpr auto routeSegs = TPath::segments;
  if (segments.size() != routeSegs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < routeSegs.size(); ++i) {
    if (segments[i] != routeSegs[i]) {
      return false;
    }
  }
  return true;
}

template <typename... Routes> class Dispatcher {
public:
  explicit Dispatcher(State &state) : state_(state) {}

  void dispatch(const Request &req) {
    auto segments = splitPath(req.path);

    bool handled = false;

    (tryRoute<Routes>(req, segments, handled), ...);

    if (!handled) {
      spdlog::error("No route matched: {} {}", static_cast<int>(req.verb),
                    req.path);
      throw std::runtime_error("Route not found");
    }
  }

private:
  State &state_;

  template <typename RouteT>
  void tryRoute(const Request &req,
                const std::vector<std::string_view> &segments, bool &handled) {
    if (handled) {
      return;
    }

    if (req.verb != RouteT::verb) {
      return;
    }

    if (!matchPath<typename RouteT::PathType>(segments)) {
      return;
    }

    spdlog::info("Matched route for path: {}", req.path);

    using ControllerT = typename RouteT::ControllerType;
    ControllerT controller{state_};

    controller.template handle<typename RouteT::Schema>(req.payload);
    handled = true;
  }
};

} // namespace owl

#endif // OWL_MQ_ROUTING_DISPATCHER