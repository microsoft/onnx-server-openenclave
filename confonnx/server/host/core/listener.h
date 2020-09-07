// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "routes.h"
#include "util.h"

namespace onnxruntime {
namespace server {

namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

// Listens on a socket and creates an HTTP session
class Listener : public std::enable_shared_from_this<Listener> {
  Routes routes_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  const tcp::endpoint endpoint_;

 public:
  Listener(const Routes& routes, net::io_context& ioc, const tcp::endpoint& endpoint);

  // Initialize the HTTP server
  bool Init();

  // Start accepting incoming connections
  bool Run();

  // Asynchronously accepts the socket
  void DoAccept();

  // Creates the HTTP session and runs it
  void OnAccept(beast::error_code ec);
};

}  // namespace server
}  // namespace onnxruntime
