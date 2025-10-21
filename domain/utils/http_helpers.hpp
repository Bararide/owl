#ifndef VECTORFS_UTILS_HTTP_HELPERS_HPP
#define VECTORFS_UTILS_HTTP_HELPERS_HPP

#include "vectorfs.hpp"
#include <functional>
#include <infrastructure/notification.hpp>
#include <map>
#include <string>
#include <vector>

#include <pistache/endpoint.h>
#include <pistache/http.h>
#include <pistache/net.h>
#include <pistache/router.h>
#include <json/json.h>

namespace owl::utils {

namespace http {
enum StatusCode {
  OK = 200,
  BadRequest = 400,
  NotFound = 404,
  InternalServerError = 500
};
}

using HttpSuccess = core::utils::Success<Json::Value>;
using HttpError = core::utils::Error;
using HttpResult = core::Result<Json::Value, std::runtime_error>;

struct HttpRequest {
  std::string method;
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> query_params;

  std::string getMethodString() const { return method; }
  std::string getPath() const { return path; }
  std::string getBody() const { return body; }

  template <typename T> T getParameter(const std::string &key) const {
    auto it = query_params.find(key);
    if (it != query_params.end()) {
      return it->second;
    }
    return T{};
  }
};

struct HttpResponse {
  Json::Value data;
  int status_code = http::OK;
  std::map<std::string, std::string> headers;

  static std::shared_ptr<HttpResponse> newHttpResponse() {
    return std::make_shared<HttpResponse>();
  }

  static std::shared_ptr<HttpResponse>
  newHttpJsonResponse(const Json::Value &json) {
    auto resp = std::make_shared<HttpResponse>();
    resp->data = json;
    resp->headers["Content-Type"] = "application/json";
    return resp;
  }

  void setStatusCode(int code) { status_code = code; }
  void setBody(const std::string &body) {
    data["_body"] = body;
  }
  void addHeader(const std::string &key, const std::string &value) {
    headers[key] = value;
  }
};

using Callback = std::function<void(const std::shared_ptr<HttpResponse> &)>;
using HttpHandler =
    std::function<void(const std::shared_ptr<HttpRequest> &, Callback &&)>;

template <typename Handler>
concept ReturnsResult =
    requires(Handler h, const std::shared_ptr<HttpRequest> &req,
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

auto create_handler(ReturnsResult auto handler_logic) {
  return [handler_logic = std::move(handler_logic)](
             const std::shared_ptr<HttpRequest> &req, Callback &&callback) {
    auto result = handler_logic(req, {});

    result.match(
        [&callback](const Json::Value &data) {
          notify_success(data);
          auto resp = HttpResponse::newHttpJsonResponse(data);
          callback(resp);
        },
        [&callback](const std::runtime_error &error) {
          notify_error(error.what());
          Json::Value errorJson;
          errorJson["status"] = "error";
          errorJson["error"] = error.what();
          errorJson["code"] = http::InternalServerError;

          auto resp = HttpResponse::newHttpJsonResponse(errorJson);
          resp->setStatusCode(http::InternalServerError);
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

Json::Value create_error_response(const std::string &error_message,
                                  int status = http::InternalServerError) {
  Json::Value response;
  response["status"] = "error";
  response["error"] = error_message;
  response["code"] = status;
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

} // namespace owl::utils

#endif // VECTORFS_UTILS_HTTP_HELPERS_HPP