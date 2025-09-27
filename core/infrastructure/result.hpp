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
  std::variant<T, Err> data;
  bool ok_flag;

public:
  Result() noexcept : data(T{}), ok_flag(true) {}

  template <typename U>
    requires IsConvertable<T, U>
  Result(U &&value) noexcept : data(std::forward<U>(value)), ok_flag(true) {}

  template <typename E>
    requires IsConvertable<Err, E>
  Result(E &&error) noexcept : data(std::forward<E>(error)), ok_flag(false) {}

  template <typename U>
    requires IsConvertable<T, U>
  Result(U &value) noexcept : data(value), ok_flag(true) {}

  template <typename E>
    requires IsConvertable<Err, E>
  Result(E &error) noexcept : data(error), ok_flag(false) {}

  template <typename U>
    requires IsConvertable<T, U>
  Result(const Result<U, Err> &other) : data(other.value()), ok_flag(true) {}

  template <typename E>
    requires IsConvertable<Err, E>
  Result(const Result<T, E> &other) : data(other.error()), ok_flag(false) {}

  template <typename U>
    requires IsConvertable<T, U>
  Result(Result<U, Err> &&other)
      : data(std::move(other.value())), ok_flag(true) {}

  template <typename E>
    requires IsConvertable<Err, E>
  Result(Result<T, E> &&other)
      : data(std::move(other.error())), ok_flag(false) {}

  bool is_ok() const { return ok_flag; }

  T &value() { return std::get<T>(data); }
  const T &value() const { return std::get<T>(data); }

  Err &error() { return std::get<Err>(data); }
  const Err &error() const { return std::get<Err>(data); }

  T &unwrap() {
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
      return error();
    }
    return func(value());
  }

  template <typename F> auto and_then(F &&func) -> decltype(func(value())) {
    if (!is_ok()) {
      return error();
    }
    return func(value());
  }

  template <typename OkHandler, typename ErrHandler>
  auto match(OkHandler &&ok_func, ErrHandler &&err_func) {
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