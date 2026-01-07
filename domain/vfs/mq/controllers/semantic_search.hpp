#ifndef OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH
#define OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct SemanticSearchController final
    : public Controller<SemanticSearchController> {
  using Base = Controller<SemanticSearchController>;
  using Base::Base;

  template <typename Schema, typename Event>
  auto operator()(const nlohmann::json &message) {
    spdlog::critical("WORK");
    return this->validate<Event>(message).map(
        [](const Event &ev) { return ev; });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH