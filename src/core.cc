module;
#include <cstdio>
export module bad_apple.core;

import std;
import boost;

namespace bad_apple {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::awaitable;

export struct ServerConfig {
  std::string bind_address;
  unsigned short port;
};

export using Request = http::request<http::string_body>;

export template <class Body, class Fields>
awaitable<void> send(beast::tcp_stream& stream, http::response<Body, Fields>&& res) {
  co_await http::async_write(stream, res, asio::use_awaitable);
}

export awaitable<void> send_text(
    beast::tcp_stream& stream, unsigned http_version, http::status status, std::string body, bool keep_alive = false) {
  http::response<http::string_body> res{status, http_version};
  res.set(http::field::server, "bad-apple");
  res.set(http::field::content_type, "text/plain; charset=utf-8");
  res.keep_alive(keep_alive);
  res.body() = std::move(body);
  res.prepare_payload();
  co_await send(stream, std::move(res));
}

export awaitable<void> send_binary(beast::tcp_stream& stream,
                                   unsigned http_version,
                                   http::status status,
                                   std::string_view content_type,
                                   std::span<const unsigned char> data,
                                   bool keep_alive = false) {
  http::response<http::vector_body<std::uint8_t>> res{status, http_version};
  res.set(http::field::server, "bad-apple");
  res.set(http::field::content_type, content_type);
  res.keep_alive(keep_alive);
  res.body() = std::vector<std::uint8_t>(data.begin(), data.end());
  res.prepare_payload();
  co_await send(stream, std::move(res));
}

export awaitable<void> send_not_found(beast::tcp_stream& stream, unsigned http_version) {
  co_await send_text(stream, http_version, http::status::not_found, "not found\n", false);
}

export awaitable<void> send_chunk(beast::tcp_stream& stream, std::string_view chunk) {
  co_await asio::async_write(stream.socket(), http::make_chunk(asio::buffer(chunk)), asio::use_awaitable);
}

export awaitable<void> send_last_chunk(beast::tcp_stream& stream) {
  co_await asio::async_write(stream.socket(), http::make_chunk_last(), asio::use_awaitable);
}

export awaitable<Request> read_request(beast::tcp_stream& stream, beast::flat_buffer& buffer) {
  http::request_parser<http::string_body> parser;
  parser.body_limit(1024 * 1024);
  co_await http::async_read(stream, buffer, parser, asio::use_awaitable);
  co_return parser.release();
}

export awaitable<void> serve_connection(auto&& handler, asio::ip::tcp::socket socket) {
  beast::tcp_stream stream(std::move(socket));
  beast::flat_buffer buffer;

  try {
    auto req = co_await read_request(stream, buffer);
    co_await handler(stream, std::move(req));
  } catch(const std::exception& e) {
    std::println(stderr, "[session] {}", e.what());
  }

  beast::error_code ec;
  stream.socket().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

awaitable<void> accept_loop(auto&& handler, ServerConfig cfg) {
  auto ex = co_await asio::this_coro::executor;
  asio::ip::tcp::acceptor acceptor(ex, {asio::ip::make_address(cfg.bind_address), cfg.port});

  std::println("[listen] http://{}:{}/", cfg.bind_address, cfg.port);

  for(;;) {
    auto socket = co_await acceptor.async_accept(asio::use_awaitable);
    auto remote = socket.remote_endpoint();
    std::println("[accept] {}:{}", remote.address().to_string(), remote.port());

    asio::co_spawn(ex, serve_connection(handler, std::move(socket)), [](std::exception_ptr ep) {
      if(!ep)
        return;
      try {
        std::rethrow_exception(ep);
      } catch(const std::exception& e) {
        std::println(stderr, "[spawn] {}", e.what());
      }
    });
  }
}

export int run_server(auto handler, ServerConfig cfg) {
  try {
    asio::io_context io(8);
    asio::co_spawn(io, accept_loop(std::move(handler), std::move(cfg)), [](std::exception_ptr ep) {
      if(!ep)
        return;
      try {
        std::rethrow_exception(ep);
      } catch(const std::exception& e) {
        std::println(stderr, "[fatal] {}", e.what());
      }
    });
    io.run();
  } catch(const std::exception& e) {
    std::println(stderr, "[fatal] {}", e.what());
    return 1;
  }
  return 0;
}

}  // namespace bad_apple
