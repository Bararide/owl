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
#include "shared_memory/shared_memory.hpp"
#include <memory/container_builder.hpp>
#include <spdlog/spdlog.h>

namespace owl::vectorfs {

struct State {
public:
  State(chunkees::Search &search, ContainerManager &container_manager,
        EmbedderManager<> &embedder_manager)
      : search_(search), container_manager_(container_manager),
        embedder_manager_(embedder_manager) {}

  chunkees::Search &search_;
  ContainerManager &container_manager_;
  EmbedderManager<> &embedder_manager_;

private:
};

} // namespace owl::vectorfs

#endif // VECTORFS_STATE_HPP