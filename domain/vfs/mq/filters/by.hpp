#ifndef OWL_MQ_RESOLVERS_BY
#define OWL_MQ_RESOLVERS_BY

#include "vfs/mq/resolvers/id.hpp"

namespace owl {

template <typename Validator> struct By {
  using ValidatorType = Validator;

  template <typename Schema> static auto validate(const nlohmann::json &json) {
    return Validator::template validate<Schema>(json);
  }
};

using ById = By<ByIdValidator>;

} // namespace owl

#endif // OWL_MQ_RESOLVERS_BY