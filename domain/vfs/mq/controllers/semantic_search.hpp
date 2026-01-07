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
    auto result = this->validate<Event>(message);

    if (result.is_ok()) {
      return result.unwrap();
    } else {
      spdlog::error("Validation failed: {}", result.error());
      return Event{};
    }
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH