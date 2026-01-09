// utils_simple.hpp
#ifndef OWL_FS_PROCESSOR_UTILS_SIMPLE
#define OWL_FS_PROCESSOR_UTILS_SIMPLE

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace owl {

inline bool checkPath(const std::string_view &path) { return fs::exists(path); }

inline bool isDirectory(const std::string_view &path) {
  return fs::exists(path) && fs::is_directory(path);
}

inline bool fileExists(const std::string_view &path) {
  return fs::exists(path) && fs::is_regular_file(path);
}

inline std::vector<std::string>
listSubdirectories(const std::string_view &path) {
  std::vector<std::string> result;
  if (!isDirectory(path))
    return result;

  for (const auto &entry : fs::directory_iterator(path)) {
    if (fs::is_directory(entry.path())) {
      result.push_back(entry.path().filename().string());
    }
  }
  return result;
}

inline fs::path getAbsolutePath(const std::string_view &path) {
  return fs::absolute(path);
}

inline nlohmann::json readJsonFile(const std::string_view &path) {
  std::ifstream file(path.data());
  nlohmann::json json;
  file >> json;
  return json;
}

} // namespace owl

#endif