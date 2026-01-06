#ifndef OWL_MQ_MAPPER
#define OWL_MQ_MAPPER

#include "vfs/core/loop/simple_separate_thread.hpp"
#include "vfs/mq/controllers/controllers.hpp"
#include "vfs/mq/zeromq_loop.hpp"

namespace owl {

static std::pair<Verb, std::string> mqmap(const std::string &verb_str) {
  static const std::unordered_map<std::string_view,
                                  std::pair<Verb, std::string_view>>
      routes{
          {"container_create",                {Verb::Post, "container/create"}},
          {"get_container_files",             {Verb::Get, "container/files"}},
          {"get_container_files_and_rebuild", {Verb::Get, "container/files"}},
          {"container_delete",                {Verb::Delete, "container/delete"}},
          {"file_create",                     {Verb::Post, "file/create"}},
          {"create_file",                     {Verb::Post, "file/create"}},
          {"file_delete",                     {Verb::Delete, "file/delete"}},
          {"delete_file",                     {Verb::Delete, "file/delete"}},
          {"container_stop",                  {Verb::Post, "container/stop"}},
          {"semantic_search_in_container",    {Verb::Post, "search/semantic"}},
          {"semantic_search",                 {Verb::Post, "search/semantic"}}};

  auto it = routes.find(verb_str);
  return it != routes.end()
             ? std::pair{it->second.first, std::string{it->second.second}}
             : throw std::runtime_error("Unknown command: " + verb_str);
}

template <typename Derived, typename TLoop, typename TDispatcher> class MQMapper {
public:
  MQMapper(State &state, std::shared_ptr<TLoop> loop)
      : state_(state), dispatcher_(state), loop_(std::move(loop)) {}

  template <typename... Args> void operator()(Args &&...args) {
    static_cast<Derived &>(*this)(std::forward<Args>(args)...);
  }

  std::shared_ptr<TLoop> getLoop() const { return loop_; }

protected:
  State &state_;
  TDispatcher dispatcher_;
  std::shared_ptr<TLoop> loop_;

  friend Derived;
};

} // namespace owl

#endif // OWL_MQ_MAPPER