import bad_apple.read;
import bad_apple.core;
import std;
import boost;

namespace {

inline constexpr unsigned char ogg_data[] = {
#embed "assets/bad-apple.ogg"
};

inline constexpr char html_begin_data[] = {
#embed "templates/page_begin.html"
};
inline constexpr std::string_view html_begin{html_begin_data, std::size(html_begin_data)};

inline constexpr char html_audio_data[] = {
#embed "templates/audio.html"
};
inline constexpr std::string_view html_audio{html_audio_data, std::size(html_audio_data)};

inline constexpr char html_end_data[] = {
#embed "templates/page_end.html"
};
inline constexpr std::string_view html_end{html_end_data, std::size(html_end_data)};

inline constexpr auto audio_preload_delay = std::chrono::seconds(3);
inline constexpr auto frame_interval = std::chrono::milliseconds(250);

}  // namespace

namespace bad_apple {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::awaitable;

awaitable<void> stream_movie(beast::tcp_stream& stream, unsigned http_version) {
  auto ex = co_await asio::this_coro::executor;
  asio::steady_timer timer(ex);

  http::response<http::empty_body> res{http::status::ok, http_version};
  res.set(http::field::server, "bad-apple");
  res.set(http::field::content_type, "text/html; charset=utf-8");
  res.chunked(true);
  res.keep_alive(false);

  http::response_serializer<http::empty_body> sr{res};
  co_await http::async_write_header(stream, sr, asio::use_awaitable);

  co_await send_chunk(stream, html_begin);

  timer.expires_after(audio_preload_delay);
  co_await timer.async_wait(asio::use_awaitable);

  co_await send_chunk(stream, html_audio);

  for(std::string_view frame_chunk : chunks) {
    co_await send_chunk(stream, frame_chunk);

    timer.expires_after(frame_interval);
    co_await timer.async_wait(asio::use_awaitable);
  }

  co_await send_chunk(stream, html_end);
  co_await send_last_chunk(stream);
}
awaitable<void> handle_request(beast::tcp_stream& stream, Request req) {
  std::println("[request] {} {} HTTP/{}.{}", std::string_view(req.method_string()), req.target(), req.version() / 10, req.version() % 10);

  if(req.method() != http::verb::get) {
    co_await send_text(stream, req.version(), http::status::method_not_allowed, "Only GET is supported\n", false);
    co_return;
  }

  const auto target = req.target();

  if(target == "/bad_apple" || target == "/bad_apple/") {
    co_await stream_movie(stream, req.version());
    co_return;
  }

  if(target == "/bad_apple/bad-apple.ogg") {
    co_await send_binary(stream, req.version(), http::status::ok, "audio/ogg", std::span<const unsigned char>{ogg_data}, false);
    co_return;
  }

  co_await send_not_found(stream, req.version());
}

}  // namespace bad_apple

int main() {
  return bad_apple::run_server(bad_apple::handle_request, {.bind_address = "0.0.0.0", .port = 8001});
}
