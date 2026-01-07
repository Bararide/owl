#ifndef OWL_MQ_ROUTING_CORE
#define OWL_MQ_ROUTING_CORE

#include <array>
#include <nlohmann/json.hpp>
#include <string_view>
#include <tuple>

namespace owl {

inline constexpr std::string_view containers_sv = "containers";
inline constexpr std::string_view file_sv = "file";
inline constexpr std::string_view container_sv = "container";
inline constexpr std::string_view search_sv = "search";
inline constexpr std::string_view semantic_sv = "semantic";
inline constexpr std::string_view create_sv = "create";
inline constexpr std::string_view delete_sv = "delete";
inline constexpr std::string_view stop_sv = "stop";
inline constexpr std::string_view rebuild_sv = "rebuild";
inline constexpr std::string_view files_sv = "files";

enum class Verb { Get, Post, Put, Delete };

template <std::string_view const &...Segs> struct Path {
  static constexpr std::array<std::string_view, sizeof...(Segs)> segments{
      Segs...};
};

template <Verb V, typename SchemaT, typename EventT, typename TPath,
          typename ControllerT, typename FilterT = void>
struct Route {
  static constexpr Verb verb = V;
  using Schema = SchemaT;
  using Event = EventT;
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