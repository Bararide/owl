#ifndef CORE_INFRASTRUCTURE_CONCEPTS_HPP
#define CORE_INFRASTRUCTURE_CONCEPTS_HPP

#include <concepts>
#include <functional>
#include <iostream>
#include <type_traits>

namespace core {
template <typename T>
concept Showable = requires(T &t, std::ostream &os) {
  { os << t } -> std::same_as<std::ostream &>;
};

template <typename T, typename U>
concept IsConvertable = std::is_convertible_v<U, T>;

template <typename T>
concept Orderable = requires(const T &v, const T &u) {
  { v > u } -> std::same_as<bool>;
  { v >= u } -> std::same_as<bool>;
  { v < u } -> std::same_as<bool>;
  { v <= u } -> std::same_as<bool>;
  { v == u } -> std::same_as<bool>;
};

template <typename T>
concept OrderableAndShowable = Orderable<T> && Showable<T>;

template <typename T>
concept Stringify = requires(const T &obj) {
  requires std::same_as<T, std::string> || requires {
    { obj.toString() } -> std::same_as<std::string>;
  } || requires {
    { std::to_string(obj) } -> std::same_as<std::string>;
  };
};

template <typename T>
concept Serializable = requires(const T &t) {
  requires Stringify<T> || requires {
    { t.serialize() } -> std::same_as<std::string>;
  };
};

template <typename F, typename... Args>
concept Invocable = std::is_invocable_v<F, Args...>;

} // namespace core

#endif // CONCEPTS_HPP