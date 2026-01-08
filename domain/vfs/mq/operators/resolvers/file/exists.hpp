#ifndef OWL_MQ_OPERATORS_RESOLVERS_FILE_FILE_EXISTS
#define OWL_MQ_OPERATORS_RESOLVERS_FILE_FILE_EXISTS

#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename State, typename Event> struct FileNotExists final {
  auto operator()(State &state,
                  const std::shared_ptr<IKnowledgeContainer> &container,
                  const Event &event) const
      -> Result<std::shared_ptr<IKnowledgeContainer>> {
    auto files = container->listFiles("/");

    auto it = std::find(files.begin(), files.end(), event.path);

    if (it == files.end()) {
      return Result<std::shared_ptr<IKnowledgeContainer>>::Ok(container);
    }

    return Result<std::shared_ptr<IKnowledgeContainer>>::Error(
        std::runtime_error("File already exists: " + event.path));
  }
};

template <typename State, typename Event> struct FileExists final {
  auto operator()(State &state,
                  const std::shared_ptr<IKnowledgeContainer> &container,
                  const Event &event) const
      -> Result<std::shared_ptr<IKnowledgeContainer>> {
    auto files = container->listFiles("/");

    auto it = std::find(files.begin(), files.end(), event.path);

    if (it == files.end()) {
      return Result<std::shared_ptr<IKnowledgeContainer>>::Error(
          std::runtime_error("File not exists: " + event.path));
    }

    return Result<std::shared_ptr<IKnowledgeContainer>>::Ok(container);
  }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVORS_FILE_FILE_EXISTS