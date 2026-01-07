#ifndef OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH
#define OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH

#include "../controller.hpp"
#include "../schemas/events.hpp"

namespace owl {

struct SemanticSearchController final
    : public Controller<SemanticSearchController> {
  using Base = Controller<SemanticSearchController>;
  using Base::Base;

  template <typename Schema> auto operator()(const nlohmann::json &message) {
    return this->validate<SemanticSearchEvent>(message);
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_SEMANTIC_SEARCH