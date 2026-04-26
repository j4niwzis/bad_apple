export module bad_apple.read;
import std;

namespace bad_apple {

// NOLINTNEXTLINE
inline constexpr unsigned char embedded_file[] = {
#embed "bad-apple.frames.2bpp.bin"
};

constexpr std::span<const unsigned char> bytes{embedded_file};

constexpr std::uint32_t read_u32_le(std::span<const unsigned char> data, std::size_t offset) {
  return (static_cast<std::uint32_t>(data[offset + 0]) << 0) | (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 16) | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

export struct Header {
  std::array<char, 4> magic{};
  std::uint32_t version{};
  std::uint32_t frame_count{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t bytes_per_frame{};
};

export struct Movie {
  std::span<const unsigned char> data{};

  static constexpr std::size_t header_size = 24;

  constexpr Header header() const {
    return Header{
        .magic =
            {
                static_cast<char>(data[0]),
                static_cast<char>(data[1]),
                static_cast<char>(data[2]),
                static_cast<char>(data[3]),
            },
        .version = read_u32_le(data, 4),
        .frame_count = read_u32_le(data, 8),
        .width = read_u32_le(data, 12),
        .height = read_u32_le(data, 16),
        .bytes_per_frame = read_u32_le(data, 20),
    };
  }

  constexpr bool valid() const {
    const auto h = header();
    const auto expected_payload = static_cast<std::size_t>(h.frame_count) * static_cast<std::size_t>(h.bytes_per_frame);

    return data.size() >= header_size && h.magic == std::array<char, 4>{'B', 'A', 'P', 'P'} &&
           data.size() == header_size + expected_payload;
  }

  constexpr std::span<const unsigned char> payload() const {
    return data.subspan(header_size);
  }

  constexpr std::size_t pixel_count_per_frame() const {
    const auto h = header();
    return static_cast<std::size_t>(h.width) * static_cast<std::size_t>(h.height);
  }

  constexpr std::span<const unsigned char> frame_bytes(std::size_t frame_index) const {
    const auto h = header();
    const auto begin = frame_index * static_cast<std::size_t>(h.bytes_per_frame);
    return payload().subspan(begin, h.bytes_per_frame);
  }

  constexpr std::uint8_t pixel_code(std::size_t frame_index, std::size_t pixel_index) const {
    const auto fb = frame_bytes(frame_index);
    const auto byte_index = pixel_index / 4;
    const auto lane = pixel_index % 4;
    const auto shift = 6u - static_cast<unsigned>(lane * 2u);
    return static_cast<std::uint8_t>((fb[byte_index] >> shift) & 0b11u);
  }

  constexpr std::uint8_t pixel_code(std::size_t frame_index, std::size_t x, std::size_t y) const {
    const auto h = header();
    return pixel_code(frame_index, y * static_cast<std::size_t>(h.width) + x);
  }

  static constexpr char decode_pixel(std::uint8_t code) {
    switch(code & 0b11u) {
      case 0:
        return '#';
      case 1:
        return '=';
      case 2:
        return '+';
      default:
        return '_';
    }
  }

  constexpr char pixel_char(std::size_t frame_index, std::size_t pixel_index) const {
    return decode_pixel(pixel_code(frame_index, pixel_index));
  }

  constexpr char pixel_char(std::size_t frame_index, std::size_t x, std::size_t y) const {
    return decode_pixel(pixel_code(frame_index, x, y));
  }

  constexpr auto pixel_chars(std::size_t frame_index) const {
    return std::views::iota(std::size_t{0}, pixel_count_per_frame()) | std::views::transform([this, frame_index](std::size_t i) constexpr {
             return pixel_char(frame_index, i);
           });
  }

  constexpr auto row_chars(std::size_t frame_index, std::size_t y) const {
    const auto w = static_cast<std::size_t>(header().width);
    return std::views::iota(std::size_t{0}, w) | std::views::transform([this, frame_index, y](std::size_t x) constexpr {
             return pixel_char(frame_index, x, y);
           });
  }

  constexpr auto frame_indices() const {
    return std::views::iota(std::size_t{0}, static_cast<std::size_t>(header().frame_count));
  }
  constexpr auto frame(std::size_t frame_index) const {
    const auto h = static_cast<std::size_t>(header().height);
    return std::views::iota(std::size_t{}, h) | std::views::transform([this, frame_index](std::size_t x) {
             return row_chars(frame_index, x);
           });
  }
  constexpr auto frames() const {
    return frame_indices() | std::views::transform([&](auto i) {
             return frame(i);
           });
  }
};

constexpr Movie movie{bytes};

constexpr std::size_t frame_size = movie.header().height * movie.header().width;

static const std::array chunk_array = [] {
  static constexpr std::string_view chunk_begin = "<style>\n.badapple::before{\ncontent:\"";
  static constexpr std::string_view chunk_end = "\";\n}</style>";
  using chunk = std::array<char, frame_size + movie.header().height * 2 + chunk_begin.size() + chunk_end.size()>;  //+ height for \A
  std::array<chunk, movie.header().frame_count> response{};
  for(auto&& [chunk, frame] : std::views::zip(response, movie.frames())) {
    auto out = chunk.begin();
    out = std::ranges::copy(chunk_begin, out).out;
    for(auto row : frame) {
      out = std::ranges::copy(row, out).out;
      out = std::ranges::copy(std::string_view{"\\A"}, out).out;
    }
    out = std::ranges::copy(chunk_end, out).out;
  }
  return response;
}();

export inline constexpr auto chunks = chunk_array | std::views::transform([](auto& arr) {
                                        return std::string_view{arr.data(), arr.size()};
                                      });

}  // namespace bad_apple
