#ifndef VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP
#define VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP

#include "knowledge_container.hpp"
#include "ossec/memory/pid_container.hpp"
#include <filesystem>
#include <fstream>

namespace owl::vectorfs {

class OssecContainerAdapter : public IKnowledgeContainer {
public:
  OssecContainerAdapter(std::shared_ptr<ossec::PidContainer> container)
      : container_(std::move(container)) {}

  std::string get_id() const override {
    return container_->get_container().container_id;
  }

  std::string get_owner() const override {
    return container_->get_container().owner_id;
  }

  std::string get_namespace() const override {
    return container_->get_container().vectorfs_config.mount_namespace;
  }

  std::map<std::string, std::string> get_labels() const override {
    return container_->get_container().labels;
  }

  std::vector<std::string> list_files(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    std::vector<std::string> files;

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_directory(full_path)) {
        for (const auto &entry :
             std::filesystem::directory_iterator(full_path)) {
          files.push_back(entry.path().filename());
        }
      }
    } catch (const std::exception &e) {
      // Логируем ошибку, но возвращаем пустой список
    }

    return files;
  }

  std::string read_file(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path);
        return std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
      }
    } catch (const std::exception &e) {
      // Логируем ошибку
    }

    return "";
  }

  bool write_file(const std::string &path,
                  const std::string &content) override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      std::filesystem::create_directories(full_path.parent_path());

      std::ofstream file(full_path);
      if (file) {
        file << content;
        return true;
      }
    } catch (const std::exception &e) {
      // Логируем ошибку
    }

    return false;
  }

  bool file_exists(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;
    return std::filesystem::exists(full_path);
  }

  std::vector<std::string> semantic_search(const std::string &query,
                                           int limit) const override {
    // TODO: Интегрировать с семантическим поиском VectorFS
    return {};
  }

  std::vector<std::string>
  search_files(const std::string &pattern) const override {
    // TODO: Реализовать поиск файлов по шаблону
    return {};
  }

  bool is_available() const override { return container_->is_running(); }

  size_t get_size() const override {
    try {
      auto data_path = container_->get_container().data_path;
      return std::filesystem::file_size(data_path);
    } catch (...) {
      return 0;
    }
  }

  std::string get_status() const override {
    if (container_->is_running()) {
      return "running";
    } else if (container_->is_owned()) {
      return "stopped";
    } else {
      return "unknown";
    }
  }

  std::shared_ptr<ossec::PidContainer> get_native_container() const {
    return container_;
  }

private:
  std::shared_ptr<ossec::PidContainer> container_;
};

} // namespace owl::vectorfs

#endif // VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP