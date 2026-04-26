import bad_apple.read;
import bad_apple.core;
import std;

inline constexpr unsigned char embedded_bad_apple_ogg[] = {
#embed "assets/bad-apple.ogg"
};
inline constexpr std::size_t embedded_bad_apple_ogg_size = sizeof(embedded_bad_apple_ogg);

inline constexpr char page_begin_arr[] = {
#embed "templates/page_begin.html"
};
inline constexpr std::string_view page_begin{page_begin_arr, sizeof(page_begin_arr)};

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::awaitable;
using namespace std::chrono_literals;

namespace bad_apple {

awaitable<void> stream(beast::tcp_stream& stream, unsigned version) {
  auto ex = co_await boost::asio::this_coro::executor;

  http::response<http::empty_body> res{boost::beast::http::status::ok, version};
  res.set(http::field::server, "bad-apple");
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.keep_alive(false);
  res.chunked(true);

  http::response_serializer<http::empty_body> sr{res};
  co_await http::async_write_header(stream, sr, boost::asio::use_awaitable);

  co_await send_stream_chunk(stream, page_begin);

  asio::steady_timer timer(ex);
  for(const auto& chunk : chunks) {
    co_await send_stream_chunk(stream, chunk);

    timer.expires_after(250ms);
    co_await timer.async_wait(boost::asio::use_awaitable);
  }

  co_await send_stream_last_chunk(stream);
}

boost::asio::awaitable<void> handle_request(beast::tcp_stream& stream, request_t req) {
  std::println("[request] {} {} http/{}.{}",
               std::string_view(req.method_string()),
               std::string_view(req.target()),
               req.version() / 10,
               req.version() % 10);

  if(req.method() != http::verb::get) {
    co_await send_text_response(stream, req.version(), http::status::method_not_allowed, "only GET is supported\n", false);
    co_return;
  }

  if(req.target() == "/") {
    co_await send_text_response(stream, req.version(), http::status::ok, "Bad Apple\ntry GET /stream\n", false);
    co_return;
  }

  if(req.target() == "/stream") {
    co_await bad_apple::stream(stream, req.version());
    co_return;
  }

  if(req.target() == "/bad-apple.ogg") {
    co_await send_binary_response(
        stream, req.version(), http::status::ok, "audio/ogg", {embedded_bad_apple_ogg, embedded_bad_apple_ogg_size}, false);
    co_return;
  }

  co_await send_not_found(stream, req.version());
}

}  // namespace bad_apple

int main() {
  return bad_apple::run_server(bad_apple::handle_request, {.bind_address = "0.0.0.0", .port = 8000});
}
