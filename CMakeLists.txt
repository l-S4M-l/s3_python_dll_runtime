cmake_minimum_required(VERSION 3.20)

project(python_runner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(python_runner SHARED
    src/dllmain.cpp
    src/python_runner.cpp
    src/tcp_server.cpp
    src/memory_api.cpp
    src/python_launcher.cpp
    src/logger.cpp
    src/path_utils.cpp
)

target_compile_definitions(python_runner PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_libraries(python_runner PRIVATE Ws2_32)

set_target_properties(python_runner PROPERTIES
    OUTPUT_NAME "python_runner"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
