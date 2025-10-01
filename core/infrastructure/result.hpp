#ifndef CORE_INFRASTRUCTURE_RESULT_HPP
#define CORE_INFRASTRUCTURE_RESULT_HPP

#include "concepts.hpp"
#include <concepts>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>
#include <variant>

namespace core {
template <typename T, typename Err = std::runtime_error> class Result {
private:
  std::unique_ptr<T> value_ptr;
  std::unique_ptr<Err> error_ptr;
  bool ok_flag;

public:
  Result() noexcept
    requires std::default_initializable<T>
      : value_ptr(std::make_unique<T>()), ok_flag(true) {}

  template <typename U>
    requires IsConvertable<T, U>
  Result(U &&value) noexcept
      : value_ptr(std::make_unique<T>(std::forward<U>(value))), ok_flag(true) {}

  template <typename U>
    requires std::is_same_v<std::decay_t<U>, T> &&
                 (!std::is_copy_constructible_v<T>)
  Result(U &&value) noexcept
      : value_ptr(std::make_unique<T>(std::forward<U>(value))), ok_flag(true) {}

  template <typename E>
    requires IsConvertable<Err, E>
  Result(E &&error) noexcept
      : error_ptr(std::make_unique<Err>(std::forward<E>(error))),
        ok_flag(false) {}

  bool is_ok() const { return ok_flag; }

  T &value() { return *value_ptr; }
  const T &value() const { return *value_ptr; }

  Err &error() { return *error_ptr; }
  const Err &error() const { return *error_ptr; }

  T &unwrap() {
    if (!is_ok()) {
      throw error();
    }
    return value();
  }

  const T &unwrap() const {
    if (!is_ok()) {
      throw error();
    }
    return value();
  }

  T expect(const char *message) {
    if (!is_ok()) {
      throw std::runtime_error(message);
    }
    return value();
  }

  template <typename F>
  auto map(F &&func) -> Result<decltype(func(value())), Err> {
    if (!is_ok()) {
      return Result<decltype(func(value())), Err>::Error(error());
    }
    return Result<decltype(func(value())), Err>::Ok(func(value()));
  }

  template <typename F> auto and_then(F &&func) -> decltype(func(value())) {
    if (!is_ok()) {
      return decltype(func(value()))::Error(error());
    }
    return func(value());
  }

  template <typename OkHandler, typename ErrHandler>
  auto match(OkHandler &&ok_func,
             ErrHandler &&err_func) -> decltype(ok_func(value())) {
    if (is_ok()) {
      return ok_func(value());
    }
    return err_func(error());
  }

  static Result Ok(T &&value) { return Result(std::forward<T>(value)); }

  static Result Ok(const T &value) { return Result(value); }

  static Result Ok() { return Result(); }

  static Result Error(Err &&error) { return Result(std::forward<Err>(error)); }

  static Result Error(const Err &error) { return Result(error); }

  static Result Error() { return Result(Err{"Unknown error"}); }

  static Result Error(const char *message) { return Result(Err{message}); }
};

} // namespace core

#endif // RESULT_HPP