#ifndef OWL_STORAGE
#define OWL_STORAGE

#include <infrastructure/event.hpp>
#include <infrastructure/result.hpp>

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

  core::Result<T &> findFile(const std::string &path) {
    auto result = file_storage_.find(path);
    if (result != file_storage_.end()) {
      return core::Result<T &>::Ok(result);
    }

    return core::Result<T &>::Error("Not find file in storage with path: {}",
                                    path);
  }

  core::Result<std::string> findDir(const std::string &path) {
    if (dirs_storage_.find(path) != dirs_storage_.end()) {
      return core::Result<std::string>::Ok(dirs_storage_.find(path));
    }

    return core::Result<std::string>::Error("Not find dir");
  }

  core::Result<T &> operator[](const std::string &path) {
    auto result = file_storage_.find(path);
    if (result != file_storage_.end()) {
      return core::Result<T &>::Ok(result);
    }

    return core::Result<T &>::Error("Not find file in storage with path: {}",
                                    path);
  }

private:
  std::map<std::string, T> file_storage_;
  std::set<std::string> dirs_storage_;
};

} // namespace owl::storage

#endif // OWL_STORAGE