#ifndef OWL_FS_PROCESSOR_UTILS
#define OWL_FS_PROCESSOR_UTILS

#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <infrastructure/result.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace owl {

inline core::Result<bool> checkPath(const std::string_view &path) {
  if (fs::exists(path)) {
    return core::Result<bool>::Ok(true);
  }

  return core::Result<bool>::Error(
      fmt::format("Path does not exist: {}", path));
}

inline core::Result<bool> dirIsEmpty(const std::string_view &path) {
  return checkPath(path).and_then([path]() -> core::Result<bool> {
    if (!fs::is_directory(path)) {
      return core::Result<bool>::Error(
          fmt::format("Is not a directory: {}", path));
    }

    if (fs::directory_iterator(path) == fs::directory_iterator()) {
      return core::Result<bool>::Ok(true);
    }

    return core::Result<bool>::Ok(false);
  });
}

inline core::Result<bool> isDirectory(const std::string_view &path) {
  return checkPath(path).map([path]() { return fs::is_directory(path); });
}

inline core::Result<bool> isRegularFile(const std::string_view &path) {
  return checkPath(path).map([path]() { return fs::is_regular_file(path); });
}

inline core::Result<bool> fileExists(const std::string_view &path) {
  return checkPath(path).and_then([path]() {
    if (fs::is_regular_file(path)) {
      return core::Result<bool>::Ok(true);
    }
    return core::Result<bool>::Error(
        fmt::format("Path exists but is not a regular file: {}", path));
  });
}

inline core::Result<std::string>
readFileToString(const std::string_view &path) {
  return fileExists(path).and_then([path]() -> core::Result<std::string> {
    std::ifstream file(path.data());
    if (!file.is_open()) {
      return core::Result<std::string>::Error(
          fmt::format("Failed to open file: {}", path));
    }

    std::string content;
    std::getline(file, content, '\0');

    if (file.fail() && !file.eof()) {
      return core::Result<std::string>::Error(
          fmt::format("Failed to read file: {}", path));
    }

    return core::Result<std::string>::Ok(content);
  });
}

inline core::Result<std::vector<std::string>>
listDirectoryEntries(const std::string_view &path) {
  return isDirectory(path).and_then(
      [path]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> entries;

        try {
          for (const auto &entry : fs::directory_iterator(path)) {
            entries.push_back(entry.path().filename().string());
          }
          return core::Result<std::vector<std::string>>::Ok(entries);
        } catch (const std::exception &e) {
          return core::Result<std::vector<std::string>>::Error(
              std::runtime_error(fmt::format("Failed to list directory {}: {}",
                                             path, e.what())));
        }
      });
}

inline core::Result<std::vector<std::string>>
listSubdirectories(const std::string_view &path) {
  return isDirectory(path).and_then(
      [path]() -> core::Result<std::vector<std::string>> {
        std::vector<std::string> subdirs;

        try {
          for (const auto &entry : fs::directory_iterator(path)) {
            if (fs::is_directory(entry.path())) {
              subdirs.push_back(entry.path().filename().string());
            }
          }
          return core::Result<std::vector<std::string>>::Ok(subdirs);
        } catch (const std::exception &e) {
          return core::Result<std::vector<std::string>>::Error(
              std::runtime_error(fmt::format(
                  "Failed to list subdirectories in {}: {}", path, e.what())));
        }
      });
}

inline core::Result<fs::path> getAbsolutePath(const std::string_view &path) {
  return checkPath(path).and_then([path]() -> core::Result<fs::path> {
    try {
      return core::Result<fs::path>::Ok(fs::absolute(path));
    } catch (const std::exception &e) {
      return core::Result<fs::path>::Error(std::runtime_error(fmt::format(
          "Failed to get absolute path for {}: {}", path, e.what())));
    }
  });
}

inline core::Result<nlohmann::json> readJsonFile(const std::string_view &path) {
  return readFileToString(path).and_then(
      [](const std::string &content) -> core::Result<nlohmann::json> {
        try {
          return core::Result<nlohmann::json>::Ok(
              nlohmann::json::parse(content));
        } catch (const nlohmann::json::exception &e) {
          return core::Result<nlohmann::json>::Error(std::runtime_error(
              fmt::format("Failed to parse JSON: {}", e.what())));
        }
      });
}

} // namespace owl

#endif // OWL_FS_PROCESSOR_UTILS