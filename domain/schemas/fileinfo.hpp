#ifndef OWL_SCHEMAS_FILEINFO
#define OWL_SCHEMAS_FILEINFO

#include <optional>
#include <infrastructure/result.hpp>
#include <pipeline/pipeline.hpp>

namespace owl::schemas {

struct FileInfo {
  std::optional<mode_t> mode;
  std::optional<size_t> size;
  std::optional<std::vector<uint8_t>> content;
  std::optional<uid_t> uid;
  std::optional<gid_t> gid;
  std::optional<time_t> access_time;
  std::optional<time_t> modification_time;
  std::optional<time_t> create_time;
  std::optional<std::string> name;
  std::optional<std::string> path;
  std::vector<float> embedding;
  std::vector<uint8_t> pq_codes;
  std::vector<uint8_t> sq_codes;
  bool embedding_updated{false};
  bool is_quantized{false};
  bool created{false};
};

} // namespace owl::schemas

#endif // OWL_SCHEMAS_FILEINFO
