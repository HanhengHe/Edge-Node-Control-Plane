cmake_minimum_required(VERSION 3.20)

project(proxy_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)

add_executable(proxy_scheduler
    control-plane/src/main.cpp
    control-plane/src/http_server.cpp
    control-plane/src/node_registry.cpp
    control-plane/src/health_checker.cpp
)

target_include_directories(proxy_scheduler PRIVATE
    control-plane/include
)

target_link_libraries(proxy_scheduler PRIVATE
    Boost::boost
)

target_compile_options(proxy_scheduler PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)