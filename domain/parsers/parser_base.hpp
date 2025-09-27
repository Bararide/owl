#ifndef VECTORFS_PARSER_BASE_HPP
#define VECTORFS_PARSER_BASE_HPP

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace owl::parser {

template <typename Derived> class ParserBase;

template <typename T> struct ParserTraits {
  using ParserType = void;
  static constexpr const char *extension = "Unknown";
};

template <typename Derived> class ParserBase {
public:
  bool parse(const std::string &path) {
    return static_cast<Derived *>(this)->parse(path);
  }

  virtual ~ParserBase() = default;

private:
  bool loadSegment(const std::string &segment) {
    return static_cast<Derived *>(this)->loadSegment(segment);
  }
};

} // namespace owl::parser

#endif // VECTORFS_PARSER_BASE_HPP
