#ifndef OWL_FS_PROCESSOR_PROCESSOR_BASE
#define OWL_FS_PROCESSOR_PROCESSOR_BASE

#include "utils.hpp"
#include "vfs/core/schemas/filesystem_schemas.hpp"

namespace owl {

class FSProcessor {
public:
  explicit FSProcessor(const std::string_view &base_path)
      : base_path_(base_path) {
    spdlog::info("FSProcessor created with base path: {}", base_path);
  }

  std::vector<ContainerMetadata> parseBaseDir() const {
    spdlog::info("parseBaseDir called for: {}", base_path_);

    if (!isDirectory(base_path_)) {
      spdlog::error("Base directory does not exist or is not a directory: {}",
                    base_path_);
      return {};
    }

    auto subdirs = listSubdirectories(base_path_);
    spdlog::info("Found {} subdirectories in {}", subdirs.size(), base_path_);

    std::vector<ContainerMetadata> containers;

    for (const auto &subdir : subdirs) {
      auto container_path = (fs::path(base_path_) / subdir).string();
      spdlog::info("Processing container: {} at {}", subdir, container_path);

      try {
        auto metadata = loadContainerMetadata(container_path);
        containers.push_back(std::move(metadata));
        spdlog::info("Successfully loaded container: {}", subdir);
      } catch (const std::exception &e) {
        spdlog::warn("Failed to load container {}: {}", subdir, e.what());
      }
    }

    spdlog::info("Loaded {}/{} containers", containers.size(), subdirs.size());
    return containers;
  }

private:
  std::string base_path_;

  ContainerMetadata
  loadContainerMetadata(const std::string &container_path) const {
    spdlog::info("loadContainerMetadata: {}", container_path);

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

    ContainerMetadata metadata;
    metadata.container_id = container_id;
    metadata.owner_id = config.value("owner", "unknown");
    metadata.status = config.value("status", "stopped");
    metadata.data_path = container_path;
    metadata.memory_limit = config.value("memory_limit", 512);
    metadata.storage_quota = config.value("storage_quota", 1024);
    metadata.file_limit = config.value("file_limit", 100);

    if (config.contains("commands") && config["commands"].is_array()) {
      metadata.commands = config["commands"].get<std::vector<std::string>>();
    }

    metadata.labels = {
        {"environment", config.value("environment", "development")},
        {"type", config.value("type", "default")}};

    spdlog::info("Created metadata for {}: owner={}, status={}", container_id,
                 metadata.owner_id, metadata.status);

    return metadata;
  }
};

} // namespace owl

#endif // OWL_FS_PROCESSOR_PROCESSOR_BASE