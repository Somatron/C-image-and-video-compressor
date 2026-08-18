cmake_minimum_required(VERSION 3.15)
project(ImageVideoCompressor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FFmpeg components via CMake built-in pathways
find_package(FFmpeg REQUIRED COMPONENTS avcodec avformat avutil)
find_package(cpp-httplib REQUIRED)

add_executable(compressor mainEngine.cpp)

# Link using native imported targets provided by vcpkg modules
target_link_libraries(compressor PRIVATE 
    FFmpeg::avcodec 
    FFmpeg::avformat 
    FFmpeg::avutil
    cpp-httplib::cpp-httplib
)
