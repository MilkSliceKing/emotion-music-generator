cmake_minimum_required(VERSION 3.16)
project(emotion-music-generator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------- 依赖查找 ----------

# OpenCV (必须)
find_package(OpenCV REQUIRED)

# dlib (可选，仅用于 68 关键点可视化)
find_path(DLIB_INCLUDE_DIR dlib/image_processing/frontal_face_detector.h)
find_library(DLIB_LIBRARY dlib)
if(DLIB_INCLUDE_DIR AND DLIB_LIBRARY)
    find_package(BLAS REQUIRED)
    find_package(LAPACK REQUIRED)
    add_compile_definitions(HAS_DLIB)
    message(STATUS "dlib: 已找到 (${DLIB_LIBRARY})，启用 landmark 功能")
else()
    message(WARNING "dlib 未找到，68关键点功能将不可用（不影响人脸检测和情绪识别）")
endif()

# PortAudio (可选，音频播放)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO portaudio-2.0)
if(NOT PORTAUDIO_FOUND)
    message(WARNING "PortAudio 未找到，音频播放功能将不可用。\n  安装: sudo apt install libportaudio2 portaudio19-dev")
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
)

target_link_libraries(emotion_music_generator
    ${OpenCV_LIBS}
    pthread
)

# dlib 链接（如果找到）
if(DLIB_INCLUDE_DIR AND DLIB_LIBRARY)
    target_include_directories(emotion_music_generator PRIVATE ${DLIB_INCLUDE_DIR})
    target_link_libraries(emotion_music_generator
        ${DLIB_LIBRARY}
        ${BLAS_LIBRARIES}
        ${LAPACK_LIBRARIES}
    )
endif()

# PortAudio 链接（如果找到）
if(PORTAUDIO_FOUND)
    target_include_directories(emotion_music_generator PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(emotion_music_generator ${PORTAUDIO_LIBRARIES})
    target_compile_definitions(emotion_music_generator PRIVATE HAS_PORTAUDIO)
else()
    message(STATUS "将以无音频模式编译 (PortAudio 未安装)")
endif()

# ---------- 编译选项 ----------

target_compile_options(emotion_music_generator PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(emotion_music_generator PRIVATE -O3)
    message(STATUS "Release 模式: 启用 -O3 优化")
endif()
