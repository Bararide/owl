#ifndef OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS
#define OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <infrastructure/result.hpp>
#include <spdlog/spdlog.h>

#include "ossec_fs_helpers.hpp"

namespace owl {

namespace fs = std::filesystem;

template <typename Derived>
class OssecFsMixin : public OssecFsHelpersMixin<Derived>,
                     public FileSystemContainer<Derived> {
public:
  using Error = std::runtime_error;

  core::Result<std::vector<std::string>>
  listFiles(const std::string &virtual_path) const {
    const auto &data_path =
        this->derived().getNative()->get_container().data_path;
    const auto real_path = this->normalizeVirtualPath(virtual_path);

    spdlog::info("listFiles: FUSE='{}' -> REAL='{}'", virtual_path,
                 (data_path / real_path).string());

    std::vector<std::string> files;

    if (!fs::exists(data_path) || !fs::is_directory(data_path)) {
      spdlog::warn("Directory not found: {}", data_path.string());
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(files));
    }

    try {
      for (const auto &entry : fs::directory_iterator(data_path / real_path)) {
        if (entry.is_regular_file() || entry.is_directory()) {
          files.push_back(entry.path().filename().string());
        }
      }
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(files));
    } catch (const std::exception &e) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("listFiles error: ") + e.what()));
    }
  }

  core::Result<bool> fileExists(const std::string &virtual_path) const {
    if (this->isRootVirtualPath(virtual_path)) {
      return core::Result<bool, Error>::Ok(true);
    }

    const auto full_path = this->makeFullPath(
        virtual_path, derived().getNative()->get_container().data_path);
    const bool exists = fs::exists(full_path);

    spdlog::info("file_exists: FUSE='{}' -> REAL='{}' -> exists={}",
                 virtual_path, full_path.string(), exists);

    return core::Result<bool, Error>::Ok(exists);
  }

  core::Result<bool> isDirectory(const std::string &virtual_path) const {
    const auto full_path = this->makeFullPath(
        virtual_path, derived().getNative()->get_container().data_path);

    try {
      bool is_dir = fs::exists(full_path) && fs::is_directory(full_path);
      return core::Result<bool, Error>::Ok(is_dir);
    } catch (const std::exception &e) {
      return core::Result<bool, Error>::Error(
          Error(std::string("isDirectory error: ") + e.what()));
    }
  }

  core::Result<std::string>
  getFileContent(const std::string &virtual_path) const {
    if (this->isRootVirtualPath(virtual_path)) {
      return core::Result<std::string, Error>::Ok(std::string{});
    }

    const auto full_path = this->makeFullPath(
        virtual_path, derived().getNative()->get_container().data_path);

    spdlog::info("get_file_content: FUSE='{}' -> REAL='{}'", virtual_path,
                 full_path.string());

    try {
      if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        return core::Result<std::string, Error>::Error(
            Error("file not found: " + full_path.string()));
      }

      std::ifstream file(full_path);
      std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
      return core::Result<std::string, Error>::Ok(std::move(content));
    } catch (const std::exception &e) {
      return core::Result<std::string, Error>::Error(
          Error(std::string("getFileContent error: ") + e.what()));
    }
  }

  core::Result<void> addFile(const std::string &virtual_path,
                             const std::string &content) {
    const auto full_path = this->makeFullPath(
        virtual_path, derived().getNative()->get_container().data_path);
    const auto search_path = this->normalizeVirtualPathAsRooted(virtual_path);

    try {
      fs::create_directories(full_path.parent_path());

      std::ofstream file(full_path);
      if (!file) {
        return core::Result<void, Error>::Error(
            Error("failed to open file for write: " + full_path.string()));
      }
      file << content;

      return this->derived().indexFileInSearch(search_path, content, "write");
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("addFile error: ") + e.what()));
    }
  }

  core::Result<void> removeFile(const std::string &virtual_path) {
    const auto full_path = this->makeFullPath(
        virtual_path, derived().getNative()->get_container().data_path);
    const auto search_path = this->normalizeVirtualPathAsRooted(virtual_path);

    try {
      const bool removed = fs::remove(full_path);
      if (!removed) {
        return core::Result<void, Error>::Error(
            Error("file not removed: " + full_path.string()));
      }

      return this->derived().removeFileFromSearch(search_path);
    } catch (const std::exception &e) {
      return core::Result<void, Error>::Error(
          Error(std::string("removeFile error: ") + e.what()));
    }
  }

  core::Result<std::vector<std::string>>
  searchFiles(const std::string &pattern) const {
    const auto &data_path =
        this->derived().getNative()->get_container().data_path;
    std::vector<std::string> results;

    try {
      for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
        if (!entry.is_regular_file()) {
          continue;
        }

        const std::string filename = entry.path().filename().string();
        if (filename.find(pattern) != std::string::npos) {
          results.push_back(fs::relative(entry.path(), data_path).string());
        }
      }
      return core::Result<std::vector<std::string>, Error>::Ok(
          std::move(results));
    } catch (const std::exception &e) {
      return core::Result<std::vector<std::string>, Error>::Error(
          Error(std::string("searchFiles error: ") + e.what()));
    }
  }

  core::Result<size_t> getSize() const {
    const auto &data_path =
        this->derived().getNative()->get_container().data_path;

    try {
      if (!fs::exists(data_path)) {
        return core::Result<size_t, Error>::Ok(0);
      }

      size_t total = 0;
      for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
        if (entry.is_regular_file()) {
          total += entry.file_size();
        }
      }
      return core::Result<size_t, Error>::Ok(total);
    } catch (const std::exception &e) {
      return core::Result<size_t, Error>::Error(
          Error(std::string("getSize error: ") + e.what()));
    }
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_MIXINS_OSSEC_FS