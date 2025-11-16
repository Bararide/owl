#include "vectorfs.hpp"

namespace owl::vectorfs {

void VectorFS::initialize_zeromq() {
  try {
    zmq_subscriber_ = std::make_unique<zmq::socket_t>(zmq_context_, ZMQ_SUB);
    zmq_subscriber_->bind("tcp://*:5555");
    zmq_subscriber_->set(zmq::sockopt::subscribe, "");

    zmq_subscriber_->set(zmq::sockopt::rcvtimeo, 0);

    running_ = true;
    message_thread_ = std::thread(&VectorFS::process_messages, this);

    parse_base_dir();

    spdlog::info("ZeroMQ subscriber started on tcp://*:5555");
  } catch (const zmq::error_t &e) {
    spdlog::error("Failed to initialize ZeroMQ: {}", e.what());
  }
}

void VectorFS::parse_base_dir() {
  try {
    std::string base_dir = "/home/bararide/.vectorfs/containers";
    spdlog::info("Parsing base directory: {}", base_dir);

    if (!std::filesystem::exists(base_dir)) {
      spdlog::warn("Base directory does not exist: {}", base_dir);
      return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(base_dir)) {
      if (entry.is_directory()) {
        std::string container_id = entry.path().filename().string();
        spdlog::info("Found container directory: {}", container_id);

        load_existing_container(container_id, entry.path().string());
      }
    }

    spdlog::info("Finished parsing base directory. Total containers: {}",
                 containers_.size());
  } catch (const std::exception &e) {
    spdlog::error("Error parsing base directory: {}", e.what());
  }
}

bool VectorFS::load_existing_container(const std::string &container_id,
                                       const std::string &container_path) {
  try {
    spdlog::info("Loading existing container: {} from {}", container_id,
                 container_path);

    if (containers_.find(container_id) != containers_.end()) {
      spdlog::info("Container {} already loaded", container_id);
      return true;
    }

    std::string config_file_path = container_path + "/container_config.json";

    if (!std::filesystem::exists(config_file_path)) {
      spdlog::warn("Config file not found for container {}: {}", container_id,
                   config_file_path);
      return false;
    }

    std::ifstream config_file(config_file_path);
    if (!config_file.is_open()) {
      spdlog::error("Failed to open config file: {}", config_file_path);
      return false;
    }

    nlohmann::json config;
    config_file >> config;
    config_file.close();

    std::string container_fuse_path = "/.containers/" + container_id;

    virtual_dirs.insert(container_fuse_path);
    spdlog::info("Created container directory in FUSE: {}",
                 container_fuse_path);

    time_t now = time(nullptr);

    std::string config_content = config.dump(2);
    std::string config_file_fuse_path =
        container_fuse_path + "/container_config.json";
    virtual_files[config_file_fuse_path] =
        fileinfo::FileInfo(S_IFREG | 0644, config_content.size(),
                           config_content, getuid(), getgid(), now, now, now);

    std::string policy_file_path = container_path + "/access_policy.json";
    std::string policy_content = "{}";
    if (std::filesystem::exists(policy_file_path)) {
      std::ifstream policy_file(policy_file_path);
      if (policy_file.is_open()) {
        nlohmann::json policy;
        policy_file >> policy;
        policy_content = policy.dump(2);
        policy_file.close();
      }
    }

    std::string policy_file_fuse_path =
        container_fuse_path + "/access_policy.json";
    virtual_files[policy_file_fuse_path] =
        fileinfo::FileInfo(S_IFREG | 0644, policy_content.size(),
                           policy_content, getuid(), getgid(), now, now, now);

    std::string debug_content =
        "=== Debug Info for Container: " + container_id +
        " ===\n\n"
        "Owner: " +
        config.value("owner", "unknown") +
        "\n"
        "Status: " +
        config.value("status", "unknown") +
        "\n"
        "Memory Limit: " +
        std::to_string(config.value("memory_limit", 0)) +
        " MB\n"
        "Storage Quota: " +
        std::to_string(config.value("storage_quota", 0)) +
        " MB\n"
        "File Limit: " +
        std::to_string(config.value("file_limit", 0)) +
        "\n"
        "Environment: " +
        config.value("environment", "unknown") +
        "\n"
        "Type: " +
        config.value("type", "unknown") +
        "\n"
        "Path: " +
        container_path + "\n";

    std::string debug_file_path = container_fuse_path + "/.debug";
    virtual_files[debug_file_path] =
        fileinfo::FileInfo(S_IFREG | 0444, debug_content.size(), debug_content,
                           getuid(), getgid(), now, now, now);

    std::string all_content = "=== All Files in Container: " + container_id +
                              " ===\n\n"
                              "container_config.json\n"
                              "access_policy.json\n"
                              ".debug\n"
                              ".all\n"
                              ".search/\n";

    std::string all_file_path = container_fuse_path + "/.all";
    virtual_files[all_file_path] =
        fileinfo::FileInfo(S_IFREG | 0444, all_content.size(), all_content,
                           getuid(), getgid(), now, now, now);

    virtual_dirs.insert(container_fuse_path + "/.search");

    ContainerInfo container_info;
    container_info.container_id = container_id;
    container_info.user_id = config.value("owner", "unknown");
    container_info.status = config.value("status", "stopped");
    container_info.namespace_ = "default";
    container_info.size = 0;
    container_info.available = (container_info.status == "running");

    if (config.contains("environment") && config.contains("type")) {
      container_info.labels = {{"environment", config["environment"]},
                               {"type", config["type"]}};
    }

    if (config.contains("commands")) {
      container_info.commands =
          config["commands"].get<std::vector<std::string>>();
    }

    containers_[container_id] = container_info;

    bool adapter_loaded =
        load_container_adapter(container_id, container_path, config);

    if (!adapter_loaded) {
      spdlog::warn(
          "Failed to load container adapter for: {}, creating virtual only",
          container_id);

      std::string status_content = "Container: " + container_id + "\nStatus: " +
                                   config.value("status", "unknown") + "\n";
      virtual_files[container_fuse_path + "/.status"] =
          fileinfo::FileInfo(S_IFREG | 0444, status_content.size(),
                             status_content, getuid(), getgid(), now, now, now);
    }

    spdlog::info("Successfully loaded existing container: {} (adapter: {})",
                 container_id, adapter_loaded ? "loaded" : "virtual only");
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Error loading existing container {}: {}", container_id,
                  e.what());
    return false;
  }
}

bool VectorFS::load_container_adapter(const std::string &container_id,
                                      const std::string &container_path,
                                      const nlohmann::json &config) {
  try {
    spdlog::info("Loading container adapter for: {}", container_id);

    if (container_adapters_.find(container_id) != container_adapters_.end()) {
      spdlog::info("Container adapter already loaded in local map: {}",
                   container_id);
      return true;
    }

    auto existing_in_main =
        state_.getContainerManager().get_container(container_id);
    if (existing_in_main) {
      spdlog::info("Container already registered in main storage: {}",
                   container_id);
      container_adapters_[container_id] = existing_in_main;
      return true;
    }

    auto container_builder = ossec::ContainerBuilder::create();

    std::string user_id = config.value("owner", "unknown");
    size_t memory_limit = config.value("memory_limit", 512);
    size_t storage_quota = config.value("storage_quota", 1024);
    size_t file_limit = config.value("file_limit", 100);
    bool privileged = config.value("privileged", false);
    std::string environment = config.value("environment", "development");
    std::string type = config.value("type", "default");

    std::vector<std::string> commands;
    if (config.contains("commands") && config["commands"].is_array()) {
      commands = config["commands"].get<std::vector<std::string>>();
    }

    auto container_result =
        container_builder.with_owner(user_id)
            .with_container_id(container_id)
            .with_data_path(container_path)
            .with_vectorfs_namespace("default")
            .with_supported_formats({"txt", "json", "yaml", "cpp", "py"})
            .with_vector_search(true)
            .with_memory_limit(memory_limit)
            .with_storage_quota(storage_quota)
            .with_file_limit(file_limit)
            .with_label("environment", environment)
            .with_label("type", type)
            .with_commands(commands)
            .privileged(privileged)
            .build();

    if (!container_result.is_ok()) {
      spdlog::error("Failed to build container for {}: {}", container_id,
                    container_result.error().what());
      return false;
    }

    auto container = container_result.value();
    auto pid_container =
        std::make_shared<ossec::PidContainer>(std::move(container));

    std::string status = config.value("status", "stopped");
    if (status == "running") {
      spdlog::info("Starting container: {}", container_id);
      auto start_result = pid_container->start();
      if (!start_result.is_ok()) {
        spdlog::warn("Failed to start container {}: {}", container_id,
                     start_result.error().what());
      }
    }

    auto adapter = std::make_shared<vectorfs::OssecContainerAdapter>(
        pid_container, state_.getEmbedderManager());

    adapter->initialize_markov_recommend_chain();

    bool registered = state_.getContainerManager().register_container(adapter);
    if (!registered) {
      spdlog::error("Failed to register container in manager: {}",
                    container_id);
      return false;
    }

    container_adapters_[container_id] = adapter;
    spdlog::info("Successfully loaded container adapter for: {}", container_id);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Error loading container adapter for {}: {}", container_id,
                  e.what());
    return false;
  }
}

void VectorFS::process_messages() {
  while (running_) {
    try {
      zmq::message_t message;
      auto result = zmq_subscriber_->recv(message, zmq::recv_flags::dontwait);

      if (result && message.size() > 0) {
        std::string message_str(static_cast<char *>(message.data()),
                                message.size());

        try {
          auto json_msg = nlohmann::json::parse(message_str);
          std::string message_type = json_msg.value("type", "");

          spdlog::info("Received ZeroMQ message type: {}", message_type);

          if (message_type == "container_create") {
            handle_container_create(json_msg);
          } else if (message_type == "file_create") {
            handle_file_create(json_msg);
          } else if (message_type == "file_delete") {
            handle_file_delete(json_msg);
          } else if (message_type == "container_stop") {
            handle_container_stop(json_msg);
          } else if (message_type == "container_delete") {
            handle_container_delete(json_msg);
          } else {
            spdlog::warn("Unknown message type: {}", message_type);
          }
        } catch (const nlohmann::json::exception &e) {
          spdlog::error("Failed to parse JSON message: {}", e.what());
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    } catch (const zmq::error_t &e) {
      if (e.num() != EAGAIN) {
        spdlog::error("ZeroMQ error: {}", e.what());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    } catch (const std::exception &e) {
      spdlog::error("Exception in ZeroMQ processing: {}", e.what());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

void VectorFS::handle_container_create(const nlohmann::json &message) {
  try {
    spdlog::info("=== FUSE: Processing container creation request ===");
    spdlog::info("Container ID: {}",
                 message["container_id"].get<std::string>());

    if (create_container_from_message(message)) {
      spdlog::info("=== FUSE: Container created successfully ===");

      spdlog::info("Total containers in FUSE: {}", containers_.size());
      for (const auto &[id, info] : containers_) {
        spdlog::info("  - Container: {} (status: {})", id, info.status);
      }
    } else {
      spdlog::error("=== FUSE: Failed to create container ===");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error handling container create: {}", e.what());
  }
}

void VectorFS::handle_file_create(const nlohmann::json &message) {
  try {
    spdlog::info("Processing file creation request");

    if (create_file_from_message(message)) {
      spdlog::info("File created successfully from ZeroMQ message");
    } else {
      spdlog::error("Failed to create file from ZeroMQ message");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error handling file create: {}", e.what());
  }
}

void VectorFS::handle_file_delete(const nlohmann::json &message) {
  try {
    spdlog::info("Processing file deletion request");

    if (delete_file_from_message(message)) {
      spdlog::info("File deleted successfully from ZeroMQ message");
    } else {
      spdlog::error("Failed to delete file from ZeroMQ message");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error handling file delete: {}", e.what());
  }
}

void VectorFS::handle_container_stop(const nlohmann::json &message) {
  try {
    spdlog::info("Processing container stop request");

    if (stop_container_from_message(message)) {
      spdlog::info("Container stopped successfully from ZeroMQ message");
    } else {
      spdlog::error("Failed to stop container from ZeroMQ message");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error handling container stop: {}", e.what());
  }
}

void VectorFS::handle_container_delete(const nlohmann::json &message) {
  try {
    spdlog::info("=== FUSE: Processing container deletion request ===");

    std::string container_id = message["container_id"];
    spdlog::info("Container ID to delete: {}", container_id);

    if (delete_container_from_message(message)) {
      spdlog::info("=== FUSE: Container deleted successfully ===");

      spdlog::info("Remaining containers in FUSE: {}", containers_.size());
      for (const auto &[id, info] : containers_) {
        spdlog::info("  - Container: {} (status: {})", id, info.status);
      }
    } else {
      spdlog::error("=== FUSE: Failed to delete container ===");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error handling container delete: {}", e.what());
  }
}

bool VectorFS::create_container_from_message(const nlohmann::json &message) {
  try {
    std::string container_id = message["container_id"];
    std::string user_id = message["user_id"];
    size_t memory_limit = message["memory_limit"];
    size_t storage_quota = message["storage_quota"];
    size_t file_limit = message["file_limit"];
    bool privileged = message["privileged"];
    std::string env_label = message["env_label"];
    std::string type_label = message["type_label"];
    auto commands = message["commands"].get<std::vector<std::string>>();

    spdlog::info("Creating container: {}", container_id);

    auto existing_container =
        state_.getContainerManager().get_container(container_id);
    if (existing_container) {
      spdlog::warn("Container already exists in main storage: {}",
                   container_id);
      return false;
    }

    std::string container_fuse_path = "/.containers/" + container_id;
    virtual_dirs.insert(container_fuse_path);

    spdlog::info("Created container directory in FUSE: {}",
                 container_fuse_path);

    time_t now = time(nullptr);

    std::string config_content = "{\n"
                                 "  \"container_id\": \"" +
                                 container_id +
                                 "\",\n"
                                 "  \"user_id\": \"" +
                                 user_id +
                                 "\",\n"
                                 "  \"status\": \"running\",\n"
                                 "  \"memory_limit\": " +
                                 std::to_string(memory_limit) +
                                 ",\n"
                                 "  \"storage_quota\": " +
                                 std::to_string(storage_quota) +
                                 ",\n"
                                 "  \"file_limit\": " +
                                 std::to_string(file_limit) +
                                 ",\n"
                                 "  \"privileged\": " +
                                 (privileged ? "true" : "false") +
                                 ",\n"
                                 "  \"environment\": \"" +
                                 env_label +
                                 "\",\n"
                                 "  \"type\": \"" +
                                 type_label +
                                 "\"\n"
                                 "}";

    std::string config_file_path =
        container_fuse_path + "/container_config.json";
    virtual_files[config_file_path] =
        fileinfo::FileInfo(S_IFREG | 0644, config_content.size(),
                           config_content, getuid(), getgid(), now, now, now);

    std::string policy_content =
        "{\n"
        "  \"container_id\": \"" +
        container_id +
        "\",\n"
        "  \"owner\": \"" +
        user_id +
        "\",\n"
        "  \"access\": \"public\",\n"
        "  \"permissions\": [\"read\", \"write\", \"search\"]\n"
        "}";

    std::string policy_file_path = container_fuse_path + "/access_policy.json";
    virtual_files[policy_file_path] =
        fileinfo::FileInfo(S_IFREG | 0644, policy_content.size(),
                           policy_content, getuid(), getgid(), now, now, now);

    auto container_builder = ossec::ContainerBuilder::create();
    auto container_result =
        container_builder.with_owner(user_id)
            .with_container_id(container_id)
            .with_data_path("/home/bararide/.vectorfs/containers/" +
                            container_id)
            .with_vectorfs_namespace("default")
            .with_supported_formats({"txt", "json", "yaml", "cpp", "py"})
            .with_vector_search(true)
            .with_memory_limit(memory_limit)
            .with_storage_quota(storage_quota)
            .with_file_limit(file_limit)
            .with_label("environment", env_label)
            .with_label("type", type_label)
            .with_commands(commands)
            .privileged(privileged)
            .build();

    if (!container_result.is_ok()) {
      spdlog::error("Failed to build container: {}",
                    container_result.error().what());
      return false;
    }

    auto container = container_result.value();
    auto pid_container =
        std::make_shared<ossec::PidContainer>(std::move(container));

    spdlog::info("Starting container...");
    auto start_result = pid_container->start();
    if (!start_result.is_ok()) {
      spdlog::error("Failed to start container: {}",
                    start_result.error().what());
      return false;
    }

    spdlog::info("Creating container adapter...");
    auto adapter = std::make_shared<vectorfs::OssecContainerAdapter>(
        pid_container, state_.getEmbedderManager());

    spdlog::info("Initializing Markov chain...");
    adapter->initialize_markov_recommend_chain();

    bool registered = state_.getContainerManager().register_container(adapter);
    if (!registered) {
      spdlog::error("Failed to register container in manager: {}",
                    container_id);
      return false;
    }

    std::string debug_content =
        "=== Debug Info for Container: " + container_id +
        " ===\n\n"
        "Owner: " +
        user_id +
        "\n"
        "Status: running\n"
        "Memory Limit: " +
        std::to_string(memory_limit) +
        " MB\n"
        "Storage Quota: " +
        std::to_string(storage_quota) +
        " MB\n"
        "File Limit: " +
        std::to_string(file_limit) +
        "\n"
        "Environment: " +
        env_label +
        "\n"
        "Type: " +
        type_label +
        "\n"
        "Commands: " +
        (commands.empty() ? "none" : "") + "\n";

    for (const auto &cmd : commands) {
      debug_content += "  - " + cmd + "\n";
    }

    std::string debug_file_path = container_fuse_path + "/.debug";
    virtual_files[debug_file_path] =
        fileinfo::FileInfo(S_IFREG | 0444, debug_content.size(), debug_content,
                           getuid(), getgid(), now, now, now);

    std::string all_content = "=== All Files in Container: " + container_id +
                              " ===\n\n"
                              "container_config.json\n"
                              "access_policy.json\n"
                              ".debug\n"
                              ".all\n";

    std::string all_file_path = container_fuse_path + "/.all";
    virtual_files[all_file_path] =
        fileinfo::FileInfo(S_IFREG | 0444, all_content.size(), all_content,
                           getuid(), getgid(), now, now, now);

    virtual_dirs.insert(container_fuse_path + "/.search");

    std::string unique_namespace = "namespace_" + container_id;

    ContainerInfo container_info;
    container_info.container_id = container_id;
    container_info.user_id = user_id;
    container_info.status = "running";
    container_info.namespace_ = "default";
    container_info.size = 0;
    container_info.available = true;
    container_info.labels = {{"environment", env_label}, {"type", type_label}};
    container_info.commands = commands;

    containers_[container_id] = container_info;
    container_adapters_[container_id] = adapter;

    spdlog::info("Successfully created and registered container: {}",
                 container_id);

    return true;

  } catch (const std::exception &e) {
    spdlog::error("Exception in create_container_from_message: {}", e.what());
    return false;
  }
}

bool VectorFS::create_file_from_message(const nlohmann::json &message) {
  try {
    std::string path = message["path"];
    std::string content = message["content"];
    std::string user_id = message["user_id"];
    std::string container_id = message["container_id"];

    spdlog::info("Creating file: {} in container: {}", path, container_id);

    if (!container_id.empty()) {
      auto container = state_.getContainerManager().get_container(container_id);
      if (!container) {
        spdlog::error("Container not found in main storage: {}", container_id);
        return false;
      }

      if (container->get_owner() != user_id) {
        spdlog::error("User {} does not have access to container {}", user_id,
                      container_id);
        return false;
      }

      auto chunks = state_.getSemanticChunker().chunk_text(content);

      int i = 0;
      for (const auto &chunk : chunks) {
        auto result =
            container->add_file(path + std::to_string(i++), chunk.text);
        if (!result) {
          spdlog::error("Failed to create file in container");
          return false;
        }
      }

      spdlog::info("File {} successfully created in container {}", path,
                   container_id);
      return true;
    } else {
      time_t now = time(nullptr);
      fileinfo::FileInfo file_info;
      file_info.mode = S_IFREG | 0644;
      file_info.size = content.size();
      file_info.content = content;
      file_info.uid = getuid();
      file_info.gid = getgid();
      file_info.access_time = now;
      file_info.modification_time = now;
      file_info.create_time = now;

      virtual_files[path] = file_info;

      auto add_result = state_.getSearch().addFileImpl(path, content);
      if (!add_result.is_ok()) {
        spdlog::warn("Failed to add file to search index: {} - {}", path,
                     add_result.error().what());
      }

      spdlog::info("File {} successfully created in virtual filesystem", path);
      return true;
    }

  } catch (const std::exception &e) {
    spdlog::error("Exception in create_file_from_message: {}", e.what());
    return false;
  }
}

bool VectorFS::delete_file_from_message(const nlohmann::json &message) {
  try {
    std::string path = message["path"];
    std::string user_id = message["user_id"];
    std::string container_id = message["container_id"];

    spdlog::info("Deleting file: {} from container: {}", path, container_id);

    if (container_id.empty()) {
      spdlog::error("Container ID is required for file deletion");
      return false;
    }

    auto container = state_.getContainerManager().get_container(container_id);
    if (!container) {
      spdlog::error("Container not found in main storage: {}", container_id);
      return false;
    }

    if (container->get_owner() != user_id) {
      spdlog::error("User {} does not have access to container {}", user_id,
                    container_id);
      return false;
    }

    if (!container->file_exists(path)) {
      spdlog::warn("File not found in container: {}", path);
      return false;
    }

    bool deleted = container->delete_file(path);
    if (deleted) {
      spdlog::info("File {} successfully deleted from container {}", path,
                   container_id);
      return true;
    } else {
      spdlog::error("Failed to delete file {} from container {}", path,
                    container_id);
      return false;
    }

  } catch (const std::exception &e) {
    spdlog::error("Exception in delete_file_from_message: {}", e.what());
    return false;
  }
}

bool VectorFS::delete_container_from_message(const nlohmann::json &message) {
  try {
    std::string container_id = message["container_id"];

    spdlog::info("=== Starting container deletion: {} ===", container_id);

    auto container = state_.getContainerManager().get_container(container_id);
    if (!container) {
      spdlog::warn("Container not found in main storage: {}", container_id);
    }

    bool unregistered =
        state_.getContainerManager().unregister_container(container_id);
    if (unregistered) {
      spdlog::info("Container unregistered from main storage: {}",
                   container_id);
    } else {
      spdlog::warn(
          "Container not found in main storage during unregistration: {}",
          container_id);
    }

    bool deleted = state_.getContainerManager().delete_container(container_id);
    if (deleted) {
      spdlog::info("Container deleted from container manager: {}",
                   container_id);
    }

    containers_.erase(container_id);
    container_adapters_.erase(container_id);

    std::string container_fuse_path = "/.containers/" + container_id;

    std::vector<std::string> files_to_remove;
    for (const auto &[file_path, _] : virtual_files) {
      if (file_path.find(container_fuse_path) == 0) {
        files_to_remove.push_back(file_path);
      }
    }

    for (const auto &file_path : files_to_remove) {
      virtual_files.erase(file_path);
      spdlog::debug("Removed virtual file: {}", file_path);
    }

    std::vector<std::string> dirs_to_remove;
    for (const auto &dir_path : virtual_dirs) {
      if (dir_path.find(container_fuse_path) == 0) {
        dirs_to_remove.push_back(dir_path);
      }
    }

    for (const auto &dir_path : dirs_to_remove) {
      virtual_dirs.erase(dir_path);
      spdlog::debug("Removed virtual directory: {}", dir_path);
    }

    virtual_dirs.erase(container_fuse_path);
    spdlog::info("Removed container directory from FUSE: {}",
                 container_fuse_path);

    std::string container_physical_path =
        "/home/bararide/.vectorfs/containers/" + container_id;
    try {
      if (std::filesystem::exists(container_physical_path)) {
        std::filesystem::remove_all(container_physical_path);
        spdlog::info("Removed physical container directory: {}",
                     container_physical_path);
      } else {
        spdlog::warn("Physical container directory not found: {}",
                     container_physical_path);
      }
    } catch (const std::filesystem::filesystem_error &e) {
      spdlog::error("Failed to remove physical container directory {}: {}",
                    container_physical_path, e.what());
    }

    spdlog::info("=== Container {} successfully deleted ===", container_id);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Exception in delete_container_from_message: {}", e.what());
    return false;
  }
}

bool VectorFS::stop_container_from_message(const nlohmann::json &message) {
  try {
    std::string container_id = message["container_id"];

    spdlog::info("Stopping container: {}", container_id);

    auto container = state_.getContainerManager().get_container(container_id);
    if (!container) {
      spdlog::warn("Container not found in main storage: {}", container_id);
      return false;
    }

    if (!state_.getContainerManager().unregister_container(container_id)) {
      spdlog::warn("Failed to unregister container from main storage: {}",
                   container_id);
    }

    auto it = containers_.find(container_id);
    if (it != containers_.end()) {
      it->second.status = "stopped";
      it->second.available = false;
    }

    container_adapters_.erase(container_id);

    spdlog::info("Container {} stopped and unregistered successfully",
                 container_id);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Exception in stop_container_from_message: {}", e.what());
    return false;
  }
}

} // namespace owl::vectorfs