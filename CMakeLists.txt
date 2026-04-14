cmake_minimum_required(VERSION 3.16)
project(emotion-music-generator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- 依赖查找 ----------

# OpenCV
find_package(OpenCV REQUIRED)

# dlib
find_path(DLIB_INCLUDE_DIR dlib/image_processing/frontal_face_detector.h)
find_library(DLIB_LIBRARY dlib)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
if(NOT DLIB_INCLUDE_DIR OR NOT DLIB_LIBRARY)
    message(FATAL_ERROR "dlib 未找到。安装: sudo apt install libdlib-dev")
endif()
message(STATUS "dlib: ${DLIB_LIBRARY}")

# ---------- 源文件 ----------

set(SOURCES
    src/main.cpp
    src/detector/face_detector.cpp
    src/detector/emotion_recognizer.cpp
    src/mapper/emotion_mapper.cpp
    src/generator/music_generator.cpp
    src/audio/audio_player.cpp
)

add_executable(emotion_music_generator ${SOURCES})

target_include_directories(emotion_music_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${DLIB_INCLUDE_DIR}
)

target_link_libraries(emotion_music_generator
    ${OpenCV_LIBS}
    ${DLIB_LIBRARY}
    ${BLAS_LIBRARIES}
    ${LAPACK_LIBRARIES}
    pthread
)

# ---------- 编译选项 ----------

target_compile_options(emotion_music_generator PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(emotion_music_generator PRIVATE -O3)
    message(STATUS "Release 模式: 启用 -O3 优化")
endif()
