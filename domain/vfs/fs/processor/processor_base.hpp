#ifndef OWL_FS_PROCESSOR_PROCESSOR_BASE
#define OWL_FS_PROCESSOR_PROCESSOR_BASE

#include "utils.hpp"
#include "vfs/core/schemas/filesystem_schemas.hpp"

namespace owl {

class FSProcessor {
public:
  explicit FSProcessor(const std::string_view &base_path)
      : base_path_(base_path) {}

  core::Result<std::vector<ContainerMetadata>> parseBaseDir() const {
    return parseBaseDirImpl(base_path_);
  }

  core::Result<std::vector<ContainerMetadata>>
  parseDir(const std::string_view &path) const {
    return parseBaseDirImpl(path);
  }

private:
  std::string base_path_;

  core::Result<std::vector<ContainerMetadata>>
  parseBaseDirImpl(const std::string_view &path) const {
    return checkPath(path).and_then([this, path]() {
      return dirIsEmpty(path).and_then(
          [this,
           path](bool empty) -> core::Result<std::vector<ContainerMetadata>> {
            if (empty) {
              spdlog::info("Base directory is empty: {}", path);
              return core::Result<std::vector<ContainerMetadata>>::Ok({});
            }
            return scanContainerDirectories(path);
          });
    });
  }

  core::Result<std::vector<ContainerMetadata>>
  scanContainerDirectories(const std::string_view &path) const {
    return listSubdirectories(path).and_then(
        [this, path](std::vector<std::string> subdirs)
            -> core::Result<std::vector<ContainerMetadata>> {
          std::vector<ContainerMetadata> containers;

          for (auto &subdir : subdirs) {
            auto container_path = (fs::path(path) / subdir).string();
            auto result = loadContainerMetadata(container_path);

            result.match(
                [&containers](ContainerMetadata container) {
                  containers.push_back(std::move(container));
                },
                [&subdir](const std::runtime_error &error) {
                  spdlog::warn("Failed to load container {}: {}", subdir,
                               error.what());
                });
          }

          return core::Result<std::vector<ContainerMetadata>>::Ok(
              std::move(containers));
        });
  }

  core::Result<ContainerMetadata>
  loadContainerMetadata(const std::string &container_path) const {
    return getAbsolutePath(container_path)
        .and_then([this, container_path](const fs::path &abs_path) {
          std::string container_id = abs_path.filename().string();

          return readJsonFile((abs_path / "container_config.json").string())
              .and_then([this, container_id, &abs_path](nlohmann::json config) {
                return createContainerMetadata(config, container_id,
                                               abs_path.string());
              });
        });
  }

  core::Result<ContainerMetadata>
  createContainerMetadata(const nlohmann::json &config,
                          const std::string &container_id,
                          const std::string &container_path) const {

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

    spdlog::info("Loaded container metadata: {} (owner: {}, status: {})",
                 metadata.container_id, metadata.owner_id, metadata.status);

    return core::Result<ContainerMetadata>::Ok(metadata);
  }
};

} // namespace owl

#endif // OWL_FS_PROCESSOR_PROCESSOR_BASE