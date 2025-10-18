#ifndef VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP
#define VECTORFS_OSSEC_CONTAINER_ADAPTER_HPP

#include "knowledge_container.hpp"
#include "search_manager.hpp"
#include <filesystem>
#include <fstream>
#include <memory/pid_container.hpp>
#include <set>

namespace owl::vectorfs {

class OssecContainerAdapter : public IKnowledgeContainer {
public:
  OssecContainerAdapter(std::shared_ptr<ossec::PidContainer> container)
      : container_(std::move(container)) {
        initialize_search_manager();
      }

  std::string get_id() const override {
    return container_->get_container().container_id;
  }

  std::string get_owner() const override {
    return container_->get_container().owner_id;
  }

  std::string get_namespace() const override {
    return container_->get_container().vectorfs_config.mount_namespace;
  }

  std::vector<std::string> get_commands() const override {
    return container_->get_container().vectorfs_config.commands;
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
          std::string filename = entry.path().filename().string();

          static const std::set<std::string> system_dirs = {
              "lost+found", "sys",      "proc",    "dev",  "boot",  "lib",
              "lib64",      "usr",      "var",     "tmp",  "run",   "mnt",
              "media",      "srv",      "opt",     "sbin", "bin",   "root",
              "home",       "etc",      "cdrom",   "snap", "lib32", "libx32",
              "srv",        "swapfile", "swap.img"};

          if (system_dirs.count(filename) > 0) {
            continue;
          }

          files.push_back(filename);
        }
      }
    } catch (const std::exception &e) {
      spdlog::error("Error listing files in {}: {}", full_path.string(),
                    e.what());
    }

    return files;
  }

  std::string get_file_content(const std::string &path) const override {
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
    }

    return "";
  }

  bool add_file(const std::string &path, const std::string &content) override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      std::filesystem::create_directories(full_path.parent_path());
      std::ofstream file(full_path);
      if (file) {
        file << content;

        if (search_manager_) {
          search_manager_->add_file(path, content);
        }

        return true;
      }
    } catch (const std::exception &e) {
      spdlog::error("Failed to add file {}: {}", path, e.what());
    }

    return false;
  }

  bool remove_file(const std::string &path) override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;

    try {
      return std::filesystem::remove(full_path);
    } catch (const std::exception &e) {
      return false;
    }
  }

  void initialize_search_manager() {
    search_manager_ = std::make_unique<SearchManager>("some text");

    search_manager_->set_content_provider([this](const std::string &path) {
      return this->get_file_content(path);
    });
    auto files = list_files("/");
    for (const auto &file : files) {
      std::string content = get_file_content("/" + file);
      if (!content.empty()) {
        search_manager_->add_file("/" + file, content);
      }
    }
  }

  bool file_exists(const std::string &path) const override {
    auto data_path = container_->get_container().data_path;
    auto full_path = data_path / path;
    return std::filesystem::exists(full_path);
  }

  std::vector<std::string> semantic_search(const std::string &query,
                                           int limit) override {
    auto results = search_manager_->semantic_search(query, limit);
    std::vector<std::string> file_paths;

    for (const auto &[file_path, score] : results) {
      file_paths.push_back(file_path);
    }

    return file_paths;
  }

  std::vector<std::string>
  search_files(const std::string &pattern) const override {
    std::vector<std::string> results;
    auto data_path = container_->get_container().data_path;

    try {
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(data_path)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();
          if (filename.find(pattern) != std::string::npos) {
            std::string relative_path =
                std::filesystem::relative(entry.path(), data_path).string();
            results.push_back(relative_path);
          }
        }
      }
    } catch (const std::exception &e) {
    }

    return results;
  }

  bool is_available() const override {
    return container_ && container_->is_running();
  }

  size_t get_size() const override {
    try {
      auto data_path = container_->get_container().data_path;
      if (std::filesystem::exists(data_path)) {
        size_t total_size = 0;
        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(data_path)) {
          if (entry.is_regular_file()) {
            total_size += entry.file_size();
          }
        }
        return total_size;
      }
    } catch (...) {
    }
    return 0;
  }

  std::string get_status() const override {
    if (!container_)
      return "invalid";
    if (container_->is_running())
      return "running";
    else if (container_->is_owned())
      return "stopped";
    else
      return "unknown";
  }

  std::shared_ptr<ossec::PidContainer> get_native_container() const {
    return container_;
  }

private:
  std::shared_ptr<ossec::PidContainer> container_;
  std::unique_ptr<SearchManager> search_manager_;
};

} // namespace owl::vectorfs

#endif