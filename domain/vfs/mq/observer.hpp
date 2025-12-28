#ifndef OWL_MQ_OBSERVER
#define OWL_MQ_OBSERVER

#include "controllers/container.hpp"
#include "controllers/file.hpp"
#include "dispatcher.hpp"
#include "routing.hpp"

namespace owl {

using ContainerFilePath = Path<containers_sv, file_sv>;

using GetContainerFileRoute =
    Route<Verb::Get, ContainerFilePath, Container, ById>;

class MQObserver {
public:
  explicit MQObserver(State &state) : dispatcher_(state) {}

  nlohmann::json onMessage(const std::string &verb_str,
                            const std::string &path,
                            const nlohmann::json &msg) {
    Verb v = parseVerb(verb_str);
    Request req{v, path, msg};
    return dispatcher_.dispatch(req);
  }

private:
  using MyDispatcher = Dispatcher<GetContainerFileRoute
                                  // , другие маршруты...
                                  >;

  MyDispatcher dispatcher_;

  static Verb parseVerb(const std::string &v) {
    if (v == "GET") {
      return Verb::Get;
    }
    if (v == "POST") {
      return Verb::Post;
    }
    if (v == "PUT") {
      return Verb::Put;
    }
    if (v == "DELETE") {
      return Verb::Delete;
    }
    throw std::runtime_error("Unknown verb: " + v);
  }
};

} // namespace owl

#endif // OWL_MQ_OBSERVER