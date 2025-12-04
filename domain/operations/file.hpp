#ifndef CONTAINER_FILE_OPERATIONS_HPP
#define CONTAINER_FILE_OPERATIONS_HPP

#include <string>
#include <vector>

namespace owl::vectorfs {

template <typename Derived> class FileOperations {
public:
  std::vector<std::string> listFiles(const std::string &path = "/") const {
    return static_cast<const Derived *>(this)->listFiles(path);
  }

  std::string getFileContent(const std::string &path) const {
    return static_cast<const Derived *>(this)->getFileContent(path);
  }

  bool addFile(const std::string &path, const std::string &content) {
    return static_cast<Derived *>(this)->addFile(path, content);
  }

  bool removeFile(const std::string &path) {
    return static_cast<Derived *>(this)->removeFile(path);
  }

  bool isDirectory(const std::string &virtual_path) const {
    return static_cast<const Derived *>(this)->isDirectory(virtual_path);
  }

  bool fileExists(const std::string &path) const {
    return static_cast<const Derived *>(this)->fileExists(path);
  }

  std::vector<std::string> searchFiles(const std::string &pattern) const {
    return static_cast<const Derived *>(this)->searchFiles(pattern);
  }
};

} // namespace owl::vectorfs

#endif // CONTAINER_FILE_OPERATIONS_HPP