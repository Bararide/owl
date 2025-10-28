#ifndef VECTORFS_STATE_HPP
#define VECTORFS_STATE_HPP

#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <search.hpp>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "container_manager.hpp"
#include "file/fileinfo.hpp"
#include "env/cppenv.hpp"
#include "shared_memory/shared_memory.hpp"
#include <memory/container_builder.hpp>
#include <spdlog/spdlog.h>

namespace owl::vectorfs {

struct State {
public:
  State(std::shared_ptr<chunkees::Search> search,
        std::shared_ptr<ContainerManager> container_manager,
        std::shared_ptr<EmbedderManager<>> embedder_manager)
      : search_(std::move(search)),
        container_manager_(std::move(container_manager)),
        embedder_manager_(std::move(embedder_manager)) {
          if(!env_manager_.load_from_file("../.env")) {
            spdlog::error("Error load env file");
          }
        }

  cppenv::EnvManager env_manager_;
  std::shared_ptr<chunkees::Search> search_;
  std::shared_ptr<ContainerManager> container_manager_;
  std::shared_ptr<EmbedderManager<>> embedder_manager_;

  chunkees::Search &get_search() {
    if (!search_)
      throw std::runtime_error("Search not initialized");
    return *search_;
  }

  ContainerManager &get_container_manager() {
    if (!container_manager_)
      throw std::runtime_error("ContainerManager not initialized");
    return *container_manager_;
  }

  EmbedderManager<> &get_embedder_manager() {
    if (!embedder_manager_)
      throw std::runtime_error("EmbedderManager<> not initialized");
    return *embedder_manager_;
  }

private:
};

} // namespace owl::vectorfs

#endif // VECTORFS_STATE_HPP