#ifndef OWL_FS_PROCESSOR_PROCESSOR_BASE
#define OWL_FS_PROCESSOR_PROCESSOR_BASE

#include "utils.hpp"

namespace owl {

class FSProcessor {
public:
  explicit FSProcessor(const std::string_view &base_path)
      : base_path_(base_path) {}

  std::vector<ossec::Container> parseBaseDir() const {
    if (!isDirectory(base_path_)) {
      spdlog::error("Base directory does not exist or is not a directory: {}",
                    base_path_);
      return {};
    }

    auto subdirs = listSubdirectories(base_path_);
    std::vector<ossec::Container> containers;

    for (const auto &subdir : subdirs) {
      auto container_path = (fs::path(base_path_) / subdir).string();

      try {
        auto container = loadContainer(container_path);
        containers.push_back(std::move(container));
      } catch (const std::exception &e) {
        spdlog::warn("Failed to load container {}: {}", subdir, e.what());
      }
    }

    return containers;
  }

private:
  std::string base_path_;

  ossec::Container loadContainer(const std::string &container_path) const {
    if (!isDirectory(container_path)) {
      throw std::runtime_error("Container path is not a directory: " +
                               container_path);
    }

    auto abs_path = getAbsolutePath(container_path);
    std::string container_id = abs_path.filename().string();

    std::string config_path = (abs_path / "container_config.json").string();
    if (!fileExists(config_path)) {
      throw std::runtime_error("Config file does not exist: " + config_path);
    }

    auto config = readJsonFile(config_path);

    ossec::Container container;

    container.container_id = container_id;
    container.owner_id = config.value("owner", "unknown");
    container.data_path = container_path;

    container.vectorfs_config.mount_namespace = container_id;

    if (config.contains("commands") && config["commands"].is_array()) {
      container.vectorfs_config.commands =
          config["commands"].get<std::vector<std::string>>();
    }

    container.resources.memory_capacity =
        config.value("memory_limit", 512) * 1024 * 1024;
    container.resources.storage_quota =
        config.value("storage_quota", 1024) * 1024 * 1024;
    container.resources.max_open_files = config.value("file_limit", 100);

    container.labels = {
        {"environment", config.value("environment", "development")},
        {"type", config.value("type", "default")},
        {"status", config.value("status", "stopped")}};

    container.cgroup_path = "/sys/fs/cgroup/vectorfs/" + container_id;

    return container;
  }
};

} // namespace owl

#endif // OWL_FS_PROCESSOR_PROCESSOR_BASE