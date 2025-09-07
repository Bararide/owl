#ifndef CORE_UTILS_ERROR_NOTIFICATION_HPP
#define CORE_UTILS_ERROR_NOTIFICATION_HPP

#include "core/infrastructure/concepts.hpp"
#include <spdlog/spdlog.h>
#include <string>

namespace core::utils {

struct Error {
public:
  explicit Error(std::string message) : message_(std::move(message)) {}
  explicit Error(const std::exception &e) : message_(e.what()) {}

  const std::string &message() const { return message_; }
  std::string &message() { return message_; }

  std::string serialize() const { return "Error: " + message_; }

  friend std::ostream &operator<<(std::ostream &os, const Error &error) {
    os << error.serialize();
    return os;
  }

private:
  std::string message_;
};

} // namespace core::utils

#endif // CORE_UTILS_ERROR_NOTIFICATION_HPP