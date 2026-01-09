#ifndef OWL_VFS_CORE_CONTAINER_FILE_SYSTEM_CONTAINER
#define OWL_VFS_CORE_CONTAINER_FILE_SYSTEM_CONTAINER

#include <infrastructure/result.hpp>

namespace owl {

template <typename Derived> class FileSystemContainer {
public:
  core::Result<std::vector<std::string>>
  listFiles(const std::string &virtual_path = "/") const {
    return derived().listFiles(virtual_path);
  }

  core::Result<bool> fileExists(const std::string &virtual_path) const {
    return derived().fileExists(virtual_path);
  }

  core::Result<bool> isDirectory(const std::string &virtual_path) const {
    return derived().isDirectory(virtual_path);
  }

  core::Result<std::string>
  getFileContent(const std::string &virtual_path) const {
    return derived().getFileContent(virtual_path);
  }

  core::Result<void> addFile(const std::string &virtual_path,
                             const std::string &content) {
    return derived().addFile(virtual_path, content);
  }

  core::Result<void> removeFile(const std::string &virtual_path) {
    return derived().removeFile(virtual_path);
  }

  core::Result<std::vector<std::string>>
  searchFiles(const std::string &pattern) const {
    return derived().searchFiles(pattern);
  }

  core::Result<size_t> getSize() const { return derived().getSize(); }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_FILE_SYSTEM_CONTAINER