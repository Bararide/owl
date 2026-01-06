#ifndef OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH
#define OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct SemanticSearchController final
    : public Controller<SemanticSearchController> {
  using Base = Controller<SemanticSearchController>;
  using Base::Base;

  template <typename Schema> void handle(const nlohmann::json &message) {
    SemanticSearchEvent event;

    event.request_id = message["request_id"];
    event.query = message["query"];
    event.limit = message.value("limit", 10);
    event.user_id = message.value("user_id", "");
    event.container_id = message.value("container_id", "");

    spdlog::info("SemanticSearchController: Searching '{}' in container {}",
                 event.query, event.container_id);

    state_.events_.Notify(std::move(event));
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH