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

    spdlog::info("ZeroMQ subscriber started on tcp://*:5555");
  } catch (const zmq::error_t &e) {
    spdlog::error("Failed to initialize ZeroMQ: {}", e.what());
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
          } else if (message_type == "container_stop") {
            handle_container_stop(json_msg);
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

    if (containers_.find(container_id) != containers_.end()) {
      spdlog::warn("Container already exists: {}", container_id);
      return false;
    }

    // 1. СНАЧАЛА СОЗДАЕМ ДИРЕКТОРИЮ В FUSE!
    std::string container_fuse_path = "/.containers/" + container_id;
    virtual_dirs.insert(container_fuse_path);
    
    spdlog::info("Created container directory in FUSE: {}", container_fuse_path);

    // 2. СОЗДАЕМ ФАЙЛЫ В FUSE
    time_t now = time(nullptr);
    
    std::string config_content = 
        "{\n"
        "  \"container_id\": \"" + container_id + "\",\n"
        "  \"user_id\": \"" + user_id + "\",\n" 
        "  \"status\": \"running\",\n"
        "  \"memory_limit\": " + std::to_string(memory_limit) + ",\n"
        "  \"storage_quota\": " + std::to_string(storage_quota) + ",\n"
        "  \"file_limit\": " + std::to_string(file_limit) + ",\n"
        "  \"privileged\": " + (privileged ? "true" : "false") + ",\n"
        "  \"environment\": \"" + env_label + "\",\n"
        "  \"type\": \"" + type_label + "\"\n"
        "}";
    
    std::string config_file_path = container_fuse_path + "/container_config.json";
    virtual_files[config_file_path] = fileinfo::FileInfo(
        S_IFREG | 0644, config_content.size(), config_content, 
        getuid(), getgid(), now, now, now
    );

    std::string policy_content = 
        "{\n"
        "  \"container_id\": \"" + container_id + "\",\n"
        "  \"owner\": \"" + user_id + "\",\n"
        "  \"access\": \"public\",\n"
        "  \"permissions\": [\"read\", \"write\", \"search\"]\n"
        "}";
    
    std::string policy_file_path = container_fuse_path + "/access_policy.json";
    virtual_files[policy_file_path] = fileinfo::FileInfo(
        S_IFREG | 0644, policy_content.size(), policy_content, 
        getuid(), getgid(), now, now, now
    );

    // 3. ТЕПЕРЬ СОЗДАЕМ КОНТЕЙНЕР С ПУТЕМ ВО ВНЕШНЕЙ ФС
    auto container_builder = ossec::ContainerBuilder::create();
    auto container_result =
        container_builder.with_owner(user_id)
            .with_container_id(container_id)
            .with_data_path("/home/bararide/.vectorfs/containers/" + container_id)
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

    spdlog::info("Container built successfully, creating PID container...");
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
        pid_container, state_.get_embedder_manager());

    spdlog::info("Initializing Markov chain...");
    adapter->initialize_markov_chain();

    bool registered = state_.get_container_manager().register_container(adapter);
    if (!registered) {
      spdlog::error("Failed to register container in manager: {}", container_id);
      return false;
    }

    std::string debug_content = 
        "=== Debug Info for Container: " + container_id + " ===\n\n"
        "Owner: " + user_id + "\n"
        "Status: running\n"
        "Memory Limit: " + std::to_string(memory_limit) + " MB\n"
        "Storage Quota: " + std::to_string(storage_quota) + " MB\n"
        "File Limit: " + std::to_string(file_limit) + "\n"
        "Environment: " + env_label + "\n"
        "Type: " + type_label + "\n"
        "Commands: " + (commands.empty() ? "none" : "") + "\n";
    
    for (const auto& cmd : commands) {
        debug_content += "  - " + cmd + "\n";
    }
    
    std::string debug_file_path = container_fuse_path + "/.debug";
    virtual_files[debug_file_path] = fileinfo::FileInfo(
        S_IFREG | 0444, debug_content.size(), debug_content, 
        getuid(), getgid(), now, now, now
    );

    std::string all_content = 
        "=== All Files in Container: " + container_id + " ===\n\n"
        "container_config.json\n"
        "access_policy.json\n"
        ".debug\n"
        ".all\n";
    
    std::string all_file_path = container_fuse_path + "/.all";
    virtual_files[all_file_path] = fileinfo::FileInfo(
        S_IFREG | 0444, all_content.size(), all_content, 
        getuid(), getgid(), now, now, now
    );

    virtual_dirs.insert(container_fuse_path + "/.search");

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

    spdlog::info("Successfully created and registered container: {}", container_id);
    
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
      auto it = container_adapters_.find(container_id);
      if (it == container_adapters_.end()) {
        spdlog::error("Container not found: {}", container_id);
        return false;
      }

      auto &container = it->second;
      auto container_info_it = containers_.find(container_id);
      if (container_info_it == containers_.end() ||
          container_info_it->second.user_id != user_id) {
        spdlog::error("User {} does not have access to container {}", user_id,
                      container_id);
        return false;
      }

      auto result = container->add_file(path, content);
      if (!result) {
        spdlog::error("Failed to create file in container");
        return false;
      }

      containers_[container_id].size += content.size();

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

      auto add_result = state_.get_search().addFileImpl(path, content);
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

bool VectorFS::stop_container_from_message(const nlohmann::json &message) {
  try {
    std::string container_id = message["container_id"];

    spdlog::info("Stopping container: {}", container_id);

    auto it = container_adapters_.find(container_id);
    if (it == container_adapters_.end()) {
      spdlog::warn("Container not found: {}", container_id);
      return false;
    }

    auto &container_adapter = it->second;

    containers_[container_id].status = "stopped";
    containers_[container_id].available = false;

    spdlog::info("Container {} stopped successfully", container_id);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("Exception in stop_container_from_message: {}", e.what());
    return false;
  }
}

} // namespace owl::vectorfs