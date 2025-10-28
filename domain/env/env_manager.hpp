#ifndef OWL_ENV_MANAGER
#define OWL_ENV_MANAGER

#include <optional>
#include <string>

namespace cppenv {

class IEnvManager {
public:
  virtual ~IEnvManager() = default;

  virtual bool load_from_file(const std::string &filename) = 0;

  virtual std::optional<std::string>
  get_value(const std::string &key) const = 0;
};

} // namespace cppenv

#endif // OWL_ENV_MANAGER
