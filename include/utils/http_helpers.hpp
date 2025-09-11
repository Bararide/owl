#ifndef VECTORFS_UTILS_HTTP_HALPERS_HPP
#define VECTORFS_UTILS_HTTP_HALPERS_HPP

#include "vectorfs.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/infrastructure/notification.hpp"

namespace vfs::utils {
using HttpSuccess = core::utils::Success<Json::Value>;
using HttpError = core::utils::Error;
using HttpResult = core::Result<Json::Value, std::runtime_error>;

template <typename Handler>
concept ReturnsResult = requires(Handler h, const drogon::HttpRequestPtr &req,
                                 const std::vector<std::string> &path) {
  { h(req, path) } -> std::same_as<HttpResult>;
};

inline auto &get_success_notification() {
  static auto notif =
      core::make_notification<HttpSuccess>([](const HttpSuccess &success) {
        spdlog::info("HTTP Success: {}", success.serialize());
      });
  return notif;
}

inline auto &get_error_notification() {
  static auto notif =
      core::make_notification<HttpError>([](const HttpError &error) {
        spdlog::error("HTTP Error: {}", error.serialize());
      });
  return notif;
}

inline void notify_success(const Json::Value &data) {
  get_success_notification()(HttpSuccess{data});
}

inline void notify_error(const std::string &message) {
  get_error_notification()(HttpError{message});
}

using HttpHandler = std::function<void(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&)>;

auto create_handler(ReturnsResult auto handler_logic) {
  return [handler_logic = std::move(handler_logic)](
             const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    auto result = handler_logic(req, {});

    result.match(
        [&callback](const Json::Value &data) {
          notify_success(data);

          Json::Value responseJson;
          responseJson["status"] = "success";
          responseJson["data"] = data;

          auto resp = drogon::HttpResponse::newHttpJsonResponse(responseJson);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          notify_error(error.what());

          Json::Value errorJson;
          errorJson["status"] = "error";
          errorJson["error"] = error.what();

          auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
          resp->setStatusCode(drogon::k500InternalServerError);
          callback(resp);
        });
  };
}

template <typename NamesContainer, typename... Args>
auto create_json_response(const NamesContainer &names, Args &&...args) {
  Json::Value response;
  auto it = std::begin(names);
  size_t index = 0;

  auto add_value = [&](auto &&value) {
    if constexpr (requires { names[index]; }) {
      response[names[index]] = std::forward<decltype(value)>(value);
    } else {
      response[*(it + index)] = std::forward<decltype(value)>(value);
    }
    index++;
  };

  (add_value(std::forward<Args>(args)), ...);
  return response;
}

template <typename... Args>
Json::Value create_success_response(Args &&...args) {
  Json::Value response;
  response["status"] = "success";

  if constexpr (sizeof...(Args) > 0) {
    response["data"] = create_json_response(std::forward<Args>(args)...);
  }

  return response;
}

template <typename T, typename... Args>
Json::Value create_success_response(std::initializer_list<T> names,
                                    Args &&...args) {
  Json::Value response;
  response["status"] = "success";

  if constexpr (sizeof...(Args) > 0) {
    response["data"] = create_json_response(names, std::forward<Args>(args)...);
  }

  return response;
}

Json::Value create_error_response(
    const std::string &error_message,
    drogon::HttpStatusCode status = drogon::k500InternalServerError) {
  Json::Value response;
  response["status"] = "error";
  response["error"] = error_message;
  response["code"] = static_cast<int>(status);
  return response;
}

auto success_result(Json::Value data) -> HttpResult {
  return HttpResult{std::move(data)};
}

auto error_result(const std::string &message) -> HttpResult {
  return HttpResult{std::runtime_error(message)};
}

auto validate_json_member(const Json::Value &json,
                          const std::string &member) -> HttpResult {
  if (!json.isMember(member)) {
    return error_result("Missing required field: " + member);
  }
  return success_result(Json::Value{});
}
} // namespace vfs::utils

#endif // VECTORFS_UTILS_HTTP_HALPERS_HPP
