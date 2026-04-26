#include <opencv2/opencv.hpp>
import std;

namespace cfg {
constexpr int target_fps = 4;
constexpr int target_width = 120;
constexpr int target_height = 42;

}  // namespace cfg

struct FileHeader {
  char magic[4] = {'B', 'A', 'P', 'P'};
  std::uint32_t version = 2;
  std::uint32_t frame_count = 0;
  std::uint32_t width = cfg::target_width;
  std::uint32_t height = cfg::target_height;
  std::uint32_t bytes_per_frame = (cfg::target_width * cfg::target_height + 3) / 4;
};

static_assert(std::is_trivially_copyable_v<FileHeader>);

[[nodiscard]] constexpr std::uint8_t encode_pixel(std::uint8_t px) noexcept {
  if(px < 64)
    return 0;  // '#'
  if(px < 128)
    return 1;  // '='
  if(px < 192)
    return 2;  // '+'
  return 3;    // '_'
}

template <class T>
[[nodiscard]] auto to_char_bytes(const T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  return std::bit_cast<std::array<char, sizeof(T)>>(value);
}

int main(int argc, const char* argv[]) {
  if(argc < 3) {
    std::println(stderr, "Usage: {} <input.mp4> <output.bin>", argv[0]);
    return 1;
  }

  constexpr std::uint32_t pixels_per_frame = cfg::target_width * cfg::target_height;

  constexpr std::uint32_t bytes_per_frame = (pixels_per_frame + 3) / 4;
  std::string_view input_video = argv[1];
  std::string_view output_file = argv[2];
  std::println("Opening video: {}", input_video);

  cv::VideoCapture cap{std::string(input_video)};
  if(!cap.isOpened()) {
    std::println(stderr, "Error: failed to open video '{}'", input_video);
    return 1;
  }

  const double source_fps = cap.get(cv::CAP_PROP_FPS);
  if(source_fps <= 0.0) {
    std::println(stderr, "Error: failed to read source FPS");
    return 1;
  }

  std::println("Source FPS: {:.3f}", source_fps);
  std::println("Target FPS: {}", cfg::target_fps);
  std::println("Target size: {}x{}", cfg::target_width, cfg::target_height);
  std::println("Pixels per frame: {}", pixels_per_frame);
  std::println("Packed bytes per frame: {}", bytes_per_frame);

  const auto estimated_frame_count = static_cast<std::uint64_t>(std::max(0.0, cap.get(cv::CAP_PROP_FRAME_COUNT)));

  const auto estimated_output_frames = static_cast<std::uint64_t>((estimated_frame_count / source_fps) * cfg::target_fps + 2);

  std::vector<char> packed_frames;
  packed_frames.reserve(estimated_output_frames * static_cast<std::uint64_t>(bytes_per_frame));

  cv::Mat frame;
  cv::Mat resized;
  cv::Mat gray;

  std::uint32_t output_frame_count = 0;
  std::uint64_t input_frame_index = 0;

  double next_sample_time = 0.0;
  const double sample_interval = 1.0 / static_cast<double>(cfg::target_fps);

  while(cap.read(frame)) {
    const double current_time = static_cast<double>(input_frame_index) / source_fps;

    if(current_time + 1e-9 >= next_sample_time) {
      cv::resize(frame, resized, cv::Size(cfg::target_width, cfg::target_height), 0.0, 0.0, cv::INTER_AREA);

      cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

      std::uint8_t packed_byte = 0;
      int pixels_in_byte = 0;

      for(int y = 0; y < gray.rows; ++y) {
        const auto* row = gray.ptr<std::uint8_t>(y);

        for(int x = 0; x < gray.cols; ++x) {
          const std::uint8_t code = encode_pixel(row[x]);
          const int shift = 6 - pixels_in_byte * 2;

          packed_byte |= static_cast<std::uint8_t>(code << shift);
          ++pixels_in_byte;

          if(pixels_in_byte == 4) {
            packed_frames.push_back(std::bit_cast<char>(packed_byte));
            packed_byte = 0;
            pixels_in_byte = 0;
          }
        }
      }

      if(pixels_in_byte != 0) {
        packed_frames.push_back(std::bit_cast<char>(packed_byte));
      }

      ++output_frame_count;
      next_sample_time += sample_interval;

      std::println("Processed frame {:>5} at {:8.3f}s", output_frame_count, current_time);
    }

    ++input_frame_index;
  }

  FileHeader header;
  header.frame_count = output_frame_count;

  std::println("Writing output: {}", output_file);

  std::ofstream out(std::string(output_file), std::ios::binary);
  if(!out) {
    std::println(stderr, "Error: failed to open output file '{}'", output_file);
    return 1;
  }

  const auto header_bytes = to_char_bytes(header);
  out.write(header_bytes.data(), static_cast<std::streamsize>(header_bytes.size()));
  out.write(packed_frames.data(), static_cast<std::streamsize>(packed_frames.size()));

  if(!out.good()) {
    std::println(stderr, "Error: failed while writing output file");
    return 1;
  }

  std::println("Done. frames={}, bytes/frame={}, total payload bytes={}",
               header.frame_count,
               header.bytes_per_frame,
               packed_frames.size());

  return 0;
}
