#ifndef OWL_VECTORFS_PIPELINE_HANDLER
#define OWL_VECTORFS_PIPELINE_HANDLER

#include <infrastructure/concepts.hpp>
#include <infrastructure/result.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace owl::pipeline {
    
template <typename Handler, typename T>
concept Handles = requires(Handler &h, const T &data) {
  { h.handle(data) } -> std::same_as<core::Result<T>>;
};

class IComparable {
public:
  virtual ~IComparable() = default;

  template <core::IsIterableAndSizable T>
  void compare(const T &data, std::string &result) {
    return doCompare(data, result);
  }

private:
  virtual void doCompare(const auto &data, std::string &result) = 0;
};

template <typename Derived> class PipelineHandler {
public:
  PipelineHandler() = default;

  template <core::IsIterableAndSizable T>
  core::Result<T> handle(const T &data) {
    static_assert(Handles<Derived, T>,
                  "Derived class must implement handle method for type T");

    return static_cast<Derived *>(this)->handle(data);
  }

protected:
  ~PipelineHandler() = default;
};

} // namespace owl::pipeline

#endif // OWL_VECTORFS_PIPELINE_HANDLER