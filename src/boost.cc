module;
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
export module boost;

namespace boost {

namespace asio {

export using asio::awaitable;
export using asio::use_awaitable;
export using asio::co_spawn;
export using asio::steady_timer;
export using asio::io_context;
export using asio::async_write;
export using asio::buffer;

namespace ip {
export using ip::tcp;
export using ip::address;
export using ip::make_address;
}  // namespace ip

namespace this_coro {
export using this_coro::executor;
}

namespace ssl {
export using ssl::stream;

}

}  // namespace asio

namespace beast {

export using beast::tcp_stream;
export using beast::flat_buffer;
export using beast::error_code;

namespace http {

export using http::async_write;
export using http::async_read;
export using http::async_write_header;
export using http::response;
export using http::field;
export using http::vector_body;
export using http::status;
export using http::string_body;
export using http::make_chunk;
export using http::make_chunk_last;
export using http::request;
export using http::request_parser;
export using http::verb;
export using http::empty_body;
export using http::response_serializer;

}  // namespace http

}  // namespace beast

}  // namespace boost

namespace std {

export using std::coroutine_traits;

}

namespace {

inline void instantiate_beast_http() {
  using namespace boost::beast::http;  // NOLINT
  request<string_body> req;
  response<dynamic_body> res;
  (void)req;
  (void)res;
}

}  // namespace
