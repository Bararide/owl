#ifndef OWL_MQ_ROUTING_CORE
#define OWL_MQ_ROUTING_CORE

#include <array>
#include <nlohmann/json.hpp>
#include <string_view>
#include <tuple>

namespace owl {

inline constexpr std::string_view containers_sv = "containers";
inline constexpr std::string_view file_sv = "file";

enum class Verb { Get, Post, Put, Delete };

template <std::string_view const &...Segs> struct Path {
  static constexpr std::array<std::string_view, sizeof...(Segs)> segments{
      Segs...};
};

template <Verb V, typename SchemaT, typename TPath, typename ControllerT, typename FilterT = void>
struct Route {
  static constexpr Verb verb = V;
  using Schema = SchemaT;
  using PathType = TPath;
  using ControllerType = ControllerT;
  using FilterType = FilterT;
};

struct Request {
  Verb verb;
  std::string path;
  nlohmann::json payload;
};

} // namespace owl

#endif // OWL_MQ_ROUTING_CORE