#ifndef CORE_INFRASTRUCTURE_RESULT_HPP
#define CORE_INFRASTRUCTURE_RESULT_HPP

#include <concepts>
#include <exception>
#include <iostream>
#include <type_traits>
#include <variant>

namespace core {
template <typename T, typename Err = std::exception> class Result {
private:
  std::variant<T, Err> data;
  bool ok_flag;

public:
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
};

} // namespace result

#endif // RESULT_HPP
