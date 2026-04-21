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

# onnxruntime
# 优先搜索系统安装路径，其次搜索 /usr/local 和 /opt
find_library(ONNXRUNTIME_LIBRARY onnxruntime
    HINTS /usr/local/lib /opt/onnxruntime/lib /usr/lib/x86_64-linux-gnu
)
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS /usr/local/include/onnxruntime /opt/onnxruntime/include /usr/include/onnxruntime
)
if(NOT ONNXRUNTIME_LIBRARY OR NOT ONNXRUNTIME_INCLUDE_DIR)
    message(FATAL_ERROR
        "onnxruntime 未找到。\n"
        "安装方法:\n"
        "  wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz\n"
        "  tar xzf onnxruntime-linux-x64-1.17.1.tgz\n"
        "  sudo cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/\n"
        "  sudo cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/\n"
        "  sudo ldconfig"
    )
endif()
message(STATUS "onnxruntime: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "onnxruntime include: ${ONNXRUNTIME_INCLUDE_DIR}")

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
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(emotion_music_generator
    ${OpenCV_LIBS}
    ${DLIB_LIBRARY}
    ${BLAS_LIBRARIES}
    ${LAPACK_LIBRARIES}
    ${ONNXRUNTIME_LIBRARY}
    pthread
)

# ---------- 编译选项 ----------

target_compile_options(emotion_music_generator PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(emotion_music_generator PRIVATE -O3)
    message(STATUS "Release 模式: 启用 -O3 优化")
endif()
