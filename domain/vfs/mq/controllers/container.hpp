#ifndef OWL_MQ_CONTROLLERS_CONTAINER
#define OWL_MQ_CONTROLLERS_CONTAINER

#include "../controller.hpp"
#include "../filters/by.hpp"
#include "../schemas/schemas.hpp"
#include "file.hpp"

namespace owl {

struct Container final : public Controller<Container> {
  using Base = Controller<Container>;
  using Base::Base;

  template <typename Schema> void handle(const nlohmann::json &message) {
    process<Schema>(message);
  }

private:
  ContainerSchemasVariant variants_;

  template <typename Schema> void process(const nlohmann::json &message) {
    auto result = ById::validate<Schema>(message);
    if (!result.is_ok()) {
      spdlog::error("Container validation failed: {}", result.error());
      return;
    }

    const auto &params = result.value();

    auto params_tuple =
        boost::hana::transform(boost::hana::keys(params), [&params](auto key) {
          return boost::hana::at_key(params, key);
        });

    boost::hana::unpack(params_tuple, [this, &message](auto &&...args) {
      next<Schema, File>(std::forward<decltype(args)>(args)..., message);
    });
  }
};

} // namespace owl

#endif // OWL_MQ_CONTROLLERS_CONTAINER