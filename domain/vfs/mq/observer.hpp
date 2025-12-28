#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "controllers/container.hpp"
#include "controllers/file.hpp"

namespace owl {

using GetContainerById = Get<Controller<Container<By<Id>>>>;

class MQObserver {
public:
private:
};

} // namespace owl

#endif // OWL_MQ_OBSERVER