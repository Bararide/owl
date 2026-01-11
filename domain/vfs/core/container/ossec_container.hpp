#ifndef OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP
#define OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <embedded/embedded_base.hpp>
#include <infrastructure/result.hpp>
#include <memory/pid_container.hpp>
#include <search/search.hpp>

#include "container_manager.hpp"
#include "container_states.hpp"

#include "mixins/ossec_fs.hpp"
#include "mixins/ossec_resource.hpp"
#include "mixins/ossec_search.hpp"
#include "mixins/ossec_state.hpp"

namespace owl {

template <typename EmbedderT = EmbedderManager<>,
          typename SearchT = chunkees::Search>
class OssecContainer
    : public OssecFsMixin<OssecContainer<EmbedderT, SearchT>>,
      public OssecResourceMixin<OssecContainer<EmbedderT, SearchT>>,
      public OssecSearchMixin<OssecContainer<EmbedderT, SearchT>>,
      public OssecStateMixin<OssecContainer<EmbedderT, SearchT>> {
public:
  using Self = OssecContainer<EmbedderT, SearchT>;
  using Error = std::runtime_error;

  OssecContainer(std::shared_ptr<ossec::PidContainer> native,
                 std::string model_path)
      : native_(std::move(native)), embedder_manager_(std::move(model_path)),
        search_(std::make_unique<SearchT>(embedder_manager_)),
        fsm_(StateVariant{container::Unknown{}}, ContainerTransitionTable{}) {
    OssecSearchMixin<Self>::initializeSearchIndexFromFs();
  }

  // ----------- IdentifiableContainer API -----------

  std::string getId() const { return native_->get_container().container_id; }

  std::string getOwner() const { return native_->get_container().owner_id; }

  std::string getNamespace() const {
    return native_->get_container().vectorfs_config.mount_namespace;
  }

  std::string getDataPath() const {
    return native_->get_container().data_path.string();
  }

  std::vector<std::string> getCommands() const {
    return native_->get_container().vectorfs_config.commands;
  }

  std::map<std::string, std::string> getLabels() const {
    return native_->get_container().labels;
  }

  // ----------- Доступ к Native / Search / Embedder -----------

  std::shared_ptr<ossec::PidContainer> getNative() const { return native_; }

  EmbedderT &embedder() { return embedder_manager_; }
  const EmbedderT &embedder() const { return embedder_manager_; }

  SearchT &search() { return *search_; }
  const SearchT &search() const { return *search_; }

private:
  std::shared_ptr<ossec::PidContainer> native_;
  EmbedderT embedder_manager_;
  std::unique_ptr<SearchT> search_;
  ContainerStateMachine fsm_;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_OSSEC_CONTAINER_HPP