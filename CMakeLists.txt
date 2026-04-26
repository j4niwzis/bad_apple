cmake_minimum_required(VERSION 4.1.1 FATAL_ERROR)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
  # This specific value changes as experimental support evolves. See
  # `Help/dev/experimental.rst` in the CMake source corresponding to
  # your CMake build for the exact value to use.
  "d0edc3af-4c50-42ea-a356-e2862fe7a444")

set(CMAKE_CXX_MODULE_STD 1)


project(
  bad_apple
  VERSION 1.0
  LANGUAGES CXX
)



if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

find_package(Boost 1.84 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

find_package(OpenCV 4 REQUIRED COMPONENTS core videoio imgproc)
add_executable(video_to_2bpp ${CMAKE_CURRENT_SOURCE_DIR}/tools/video_to_2bpp.cc)
target_compile_features(video_to_2bpp PRIVATE cxx_std_26)
target_link_libraries(video_to_2bpp
  PRIVATE
        opencv_core
        opencv_videoio
        opencv_imgproc
)

set(FRAMES_BIN "${CMAKE_CURRENT_BINARY_DIR}/bad-apple.frames.2bpp.bin")

add_custom_command(
    OUTPUT ${FRAMES_BIN}
    COMMAND video_to_2bpp
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/bad-apple.mp4"
            "${FRAMES_BIN}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/bad-apple.mp4" video_to_2bpp
    COMMENT "Converting video to 2bpp frame data..."
    VERBATIM
)


file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")


add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_26)

target_compile_options(${PROJECT_NAME} PRIVATE "-Wno-c23-extensions")


target_sources(${PROJECT_NAME} PUBLIC FILE_SET CXX_MODULES FILES ${sources})

# Link dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
  Boost::system
  OpenSSL::SSL
  Threads::Threads
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/read.cc APPEND PROPERTY
    OBJECT_DEPENDS ${FRAMES_BIN}
)



