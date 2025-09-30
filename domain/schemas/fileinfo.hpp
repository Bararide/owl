#ifndef OWL_SCHEMAS_FILEINFO
#define OWL_SCHEMAS_FILEINFO

#include <optional>

namespace owl::schemas {

struct FileInfo {
  std::optional<std::string> name;
  std::optional<std::string> path;
  std::optional<std::string> content;
  std::optional<uint32_t> size;
  bool created{false};
};

} // namespace owl::schemas

#endif // OWL_SCHEMAS_FILEINFO
