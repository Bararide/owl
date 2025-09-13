#ifndef VECTORFS_PARSER_PDF_HPP
#define VECTORFS_PARSER_PDF_HPP

#include "parser_base.hpp"

namespace vfs::parser {
class PdfParser;

template <> struct ParserTraits<PdfParser> {
  using ParserType = PdfParser;
  static constexpr const char *extension = "pdf";
};

class PdfParser : public ParserBase<PdfParser> {
public:
  PdfParser() = default;

  bool parse(const std::string &path) { return true; }

private:
  bool loadSegment(const std::string &path) { return true; }
};
} // namespace vfs::parser

#endif // VECTORFS_PARSER_PDF_HPP
