#ifndef OWL_VFS_CORE_SOCKET_SOCKET
#define OWL_VFS_CORE_SOCKET_SOCKET

#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <zmq.hpp>

namespace owl {

enum class SocketType {
  Pair = ZMQ_PAIR,
  Pub = ZMQ_PUB,
  Sub = ZMQ_SUB,
  Req = ZMQ_REQ,
  Rep = ZMQ_REP,
  Dealer = ZMQ_DEALER,
  Router = ZMQ_ROUTER,
  Pull = ZMQ_PULL,
  Push = ZMQ_PUSH,
  XPub = ZMQ_XPUB,
  XSub = ZMQ_XSUB,
  Stream = ZMQ_STREAM
};

class Socket final {
public:
  explicit Socket(SocketType type, std::string_view endpoint,
                  std::optional<int> io_threads = std::nullopt,
                  std::optional<int> max_sockets = std::nullopt)
      : context_(io_threads.value_or(1)),
        socket_(context_, static_cast<int>(type)), type_(type) {

    spdlog::critical("=== SOCKET CONSTRUCTOR ===");
    spdlog::critical("Type: {} ({}), Endpoint: {}", static_cast<int>(type),
                     type == SocketType::Pub ? "PUB" : "SUB", endpoint);

    if (isBindType(type)) {
      spdlog::critical(">>> BINDING to: {}", endpoint);
      try {
        socket_.bind(std::string(endpoint));
        spdlog::critical(">>> SUCCESSFULLY BOUND to: {}", endpoint);
      } catch (const zmq::error_t &e) {
        spdlog::critical(">>> FAILED to bind to {}: {}", endpoint, e.what());
        throw;
      }
    } else {
      spdlog::critical(">>> CONNECTING to: {}", endpoint);
      try {
        socket_.connect(std::string(endpoint));
        spdlog::critical(">>> SUCCESSFULLY CONNECTED to: {}", endpoint);
      } catch (const zmq::error_t &e) {
        spdlog::critical(">>> FAILED to connect to {}: {}", endpoint, e.what());
        throw;
      }
    }
  }

  void setIdentity(std::string_view identity) {
    socket_.set(zmq::sockopt::routing_id, std::string(identity));
  }

  void setSubscribe(std::string_view filter = "") {
    if (type_ == SocketType::Sub || type_ == SocketType::XSub) {
      socket_.set(zmq::sockopt::subscribe, std::string(filter));
    }
  }

  void setUnsubscribe(std::string_view filter) {
    if (type_ == SocketType::Sub || type_ == SocketType::XSub) {
      socket_.set(zmq::sockopt::unsubscribe, std::string(filter));
    }
  }

  void setLinger(int linger_ms) {
    socket_.set(zmq::sockopt::linger, linger_ms);
  }

  void setSendTimeout(int timeout_ms) {
    socket_.set(zmq::sockopt::sndtimeo, timeout_ms);
  }

  void setReceiveTimeout(int timeout_ms) {
    socket_.set(zmq::sockopt::rcvtimeo, timeout_ms);
  }

  void setSendBufferSize(int size) { socket_.set(zmq::sockopt::sndbuf, size); }

  void setReceiveBufferSize(int size) {
    socket_.set(zmq::sockopt::rcvbuf, size);
  }

  void setReconnectInterval(int interval_ms) {
    socket_.set(zmq::sockopt::reconnect_ivl, interval_ms);
  }

  void setReconnectIntervalMax(int max_interval_ms) {
    socket_.set(zmq::sockopt::reconnect_ivl_max, max_interval_ms);
  }

  void setMaxMessageSize(int64_t size) {
    socket_.set(zmq::sockopt::maxmsgsize, size);
  }

  void setTcpKeepAlive(int value) {
    socket_.set(zmq::sockopt::tcp_keepalive, value);
  }

  void setTcpKeepAliveIdle(int idle_sec) {
    socket_.set(zmq::sockopt::tcp_keepalive_idle, idle_sec);
  }

  void setTcpKeepAliveIntvl(int interval_sec) {
    socket_.set(zmq::sockopt::tcp_keepalive_intvl, interval_sec);
  }

  void setImmediate(bool immediate) {
    socket_.set(zmq::sockopt::immediate, immediate ? 1 : 0);
  }

  void setIPv6(bool enable) { socket_.set(zmq::sockopt::ipv6, enable ? 1 : 0); }

  bool send(zmq::message_t &&message,
            zmq::send_flags flags = zmq::send_flags::none) {
    return socket_.send(std::move(message), flags).value_or(-1);
  }

  int send(std::string_view data,
           zmq::send_flags flags = zmq::send_flags::none) {
    zmq::message_t msg(data.data(), data.size());
    return socket_.send(std::move(msg), flags).value_or(-1);
  }

  int send(const void *data, size_t size,
           zmq::send_flags flags = zmq::send_flags::none) {
    zmq::message_t msg(data, size);
    return socket_.send(std::move(msg), flags).value_or(-1);
  }

  std::optional<zmq::message_t>
  receive(zmq::recv_flags flags = zmq::recv_flags::none) {
    zmq::message_t msg;
    if (socket_.recv(msg, flags)) {
      return msg;
    }
    return std::nullopt;
  }

  std::optional<std::string>
  receiveString(zmq::recv_flags flags = zmq::recv_flags::none) {
    auto msg = receive(flags);
    if (msg) {
      return std::string(static_cast<char *>(msg->data()), msg->size());
    }
    return std::nullopt;
  }

  std::string getIdentity() const {
    return socket_.get(zmq::sockopt::routing_id);
  }

  int getLinger() const { return socket_.get(zmq::sockopt::linger); }

  int getSendTimeout() const { return socket_.get(zmq::sockopt::sndtimeo); }

  int getReceiveTimeout() const { return socket_.get(zmq::sockopt::rcvtimeo); }

  zmq::socket_t &raw() { return socket_; }
  const zmq::socket_t &raw() const { return socket_; }

  void close() { socket_.close(); }

  ~Socket() { close(); }

private:
  static bool isBindType(SocketType type) {
    switch (type) {
    case SocketType::Pub:    // Publisher BIND
    case SocketType::Sub:    // Subscriber BIND
    case SocketType::Rep:    // Reply BIND
    case SocketType::Router: // Router BIND
    case SocketType::Pull:   // Pull BIND
    case SocketType::XPub:   // XPublisher BIND
    case SocketType::Stream: // Stream BIND
      return true;
    case SocketType::Req:    // Request CONNECT
    case SocketType::Dealer: // Dealer CONNECT
    case SocketType::Push:   // Push CONNECT
    case SocketType::XSub:   // XSubscriber CONNECT
      return false;
    default:
      return true;
    }
  }

  zmq::context_t context_;
  zmq::socket_t socket_;
  SocketType type_;
};

} // namespace owl

#endif // OWL_VFS_CORE_SOCKET_SOCKET