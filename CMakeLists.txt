cmake_minimum_required(VERSION 3.16)
project(emotion-music-generator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- 依赖查找 ----------

# OpenCV
find_package(OpenCV REQUIRED)

# dlib (手动查找，兼容 apt 安装和源码安装)
find_path(DLIB_INCLUDE_DIR dlib/image_processing/frontal_face_detector.h)
find_library(DLIB_LIBRARY dlib)
if(NOT DLIB_INCLUDE_DIR OR NOT DLIB_LIBRARY)
    message(FATAL_ERROR
        "dlib 未找到。\n"
        "  安装方式: sudo apt install libdlib-dev\n"
        "  或从源码: git clone https://github.com/davisking/dlib.git && cd dlib/build && cmake .. && sudo make install"
    )
endif()
message(STATUS "dlib: ${DLIB_LIBRARY}")

# PortAudio
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(NOT PORTAUDIO_FOUND)
    message(WARNING
        "PortAudio 未找到，音频播放功能将不可用。\n"
        "  安装方式: sudo apt install libportaudio2 portaudio19-dev"
    )
endif()

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
    pthread
)

if(PORTAUDIO_FOUND)
    target_include_directories(emotion_music_generator PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(emotion_music_generator ${PORTAUDIO_LIBRARIES})
    target_compile_definitions(emotion_music_generator PRIVATE HAS_PORTAUDIO)
else()
    message(STATUS "将以无音频模式编译 (PortAudio 未安装)")
endif()

# ---------- 编译选项 ----------

target_compile_options(emotion_music_generator PRIVATE
    -Wall -Wextra -Wpedantic
)

# Release 模式 -O3 优化
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(emotion_music_generator PRIVATE -O3)
    message(STATUS "Release 模式: 启用 -O3 优化")
endif()
