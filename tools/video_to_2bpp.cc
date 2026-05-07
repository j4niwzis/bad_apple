#include <opencv2/opencv.hpp>
import std;

inline constexpr int target_fps = 4;
inline constexpr int target_width = 120;
inline constexpr int target_height = 42;
inline constexpr int bytes_per_frame = (target_width * target_height + 3) / 4;

constexpr std::uint8_t encode_pixel(std::uint8_t gray) noexcept {
  return gray / 64;  // 0..3
}

struct FileHeader {
  char magic[4] = {'B', 'A', 'P', 'P'};
  std::uint32_t version = 2;
  std::uint32_t frame_count = 0;
  std::uint32_t width = target_width;
  std::uint32_t height = target_height;
  std::uint32_t packed_size = bytes_per_frame;
};

[[nodiscard]] auto to_bytes(const auto& trivial) {
  return std::bit_cast<std::array<char, sizeof(trivial)>>(trivial);
}

void pack_gray_frame(const cv::Mat& gray, std::vector<char>& out) {
  std::uint8_t byte = 0;
  int packed = 0;

  for(int y = 0; y < gray.rows; ++y) {
    const auto* row = gray.ptr<std::uint8_t>(y);
    for(int x = 0; x < gray.cols; ++x) {
      const auto shift = 6 - packed * 2;
      byte |= static_cast<std::uint8_t>(encode_pixel(row[x]) << shift);
      if(++packed == 4) {
        out.push_back(static_cast<char>(byte));
        byte = 0;
        packed = 0;
      }
    }
  }
  if(packed != 0) {
    out.push_back(static_cast<char>(byte));
  }
}

int main(int argc, char* argv[]) {
  if(argc != 3) {
    std::println(stderr, "Usage: {} <input.mp4> <output.bin>", argv[0]);
    return 1;
  }

  std::string_view input_path = argv[1];
  std::string_view output_path = argv[2];

  cv::VideoCapture cap{std::string(input_path)};
  if(!cap.isOpened()) {
    std::println(stderr, "Error: cannot open '{}'", input_path);
    return 1;
  }

  const double source_fps = cap.get(cv::CAP_PROP_FPS);
  const auto total_frames = static_cast<std::int64_t>(cap.get(cv::CAP_PROP_FRAME_COUNT));
  const double duration = (source_fps > 0.0) ? (total_frames / source_fps) : 0.0;
  const auto expected_out = static_cast<std::uint32_t>(duration * target_fps + 2);

  std::println("Source: {} @ {:.2f} fps, ~{:.1f} s", input_path, source_fps, duration);
  std::println("Target: {}x{} @ {} fps, {} bytes/frame", target_width, target_height, target_fps, bytes_per_frame);

  std::vector<char> payload;
  payload.reserve(expected_out * bytes_per_frame);

  cv::Mat frame, resized, gray;
  std::uint32_t out_frame_count = 0;
  std::uint64_t in_idx = 0;
  const double dt = 1.0 / target_fps;
  double next_t = 0.0;

  while(cap.read(frame)) {
    const double t = in_idx / source_fps;
    if(t + 1e-9 >= next_t) {
      cv::resize(frame, resized, {target_width, target_height}, 0, 0, cv::INTER_AREA);
      cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
      pack_gray_frame(gray, payload);
      ++out_frame_count;
      next_t += dt;
    }
    ++in_idx;
  }

  FileHeader hdr;
  hdr.frame_count = out_frame_count;

  std::ofstream out(std::string(output_path), std::ios::binary);
  if(!out) {
    std::println(stderr, "Error: cannot create '{}'", output_path);
    return 1;
  }

  auto hdr_bytes = to_bytes(hdr);
  out.write(hdr_bytes.data(), static_cast<std::streamsize>(hdr_bytes.size()));
  out.write(payload.data(), static_cast<std::streamsize>(payload.size()));

  std::println("Done: {} frames, {} KiB", out_frame_count, payload.size() / 1024);
}
