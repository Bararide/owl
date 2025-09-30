#ifndef OWL_STORAGE
#define OWL_STORAGE

#include <infrastructure/result.hpp>
#include <infrastructure/event.hpp>

namespace owl::storage {

template <typename T> class Storage {
public:
  Storage() = default;
  ~Storage() = default;

  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;
  Storage(Storage &&) = delete;
  Storage &operator=(Storage &&) = delete;

  core::Result<bool> addFile(const T file) {
    if (file_storage_.find(file.name) != file_storage_.end()) {
      return core::Result<bool>::Error("File already exists");
    }

    file_storage_[file.name] = file;
    return core::Result<bool>::Ok(true);
  }

  core::Result<bool> updateFile(const T file) {
    if (file_storage_.find(file.name) == file_storage_.end()) {
      return core::Result<bool>::Error("File not exists");
    }

    file_storage_[file.name] = file;
    return core::Result<bool>::Ok(true);
  }

  core::Result<bool> deleteFile(const std::string name) {
    if (file_storage_.find(name) == file_storage_.end()) {
      return core::Result<bool>::Error("File not exists");
    }

    file_storage_.erase(name);
    return core::Result<bool>::Ok(true);
  }

private:
  std::map<std::string, T> file_storage_;
  std::set<std::string> dirs_storage_;
}
} // namespace owl::storage

#endif // OWL_STORAGE