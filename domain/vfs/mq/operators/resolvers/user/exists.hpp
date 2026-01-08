#ifndef OWL_MQ_OPERATORS_RESOLVERS_USER_EXISTS
#define OWL_MQ_OPERATORS_RESOLVERS_USER_EXISTS

#include "vfs/mq/operators/resolvers/resolver.hpp"

namespace owl {

template <typename State, typename Event> struct UserExists {
  auto operator()(State &state, const Event &event) const
      -> Result<std::shared_ptr<IKnowledgeContainer>, std::runtime_error> {
        
      }
};

} // namespace owl

#endif // OWL_MQ_OPERATORS_RESOLVERS_USER_EXISTS