cmake_minimum_required(VERSION 3.20)

project(proxy_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS json)

add_executable(proxy_scheduler
    src/main.cpp
    src/http_server.cpp
    src/node_registry.cpp
    src/health_checker.cpp
    src/scheduler.cpp
)

target_include_directories(proxy_scheduler PRIVATE
    include
)

target_link_libraries(proxy_scheduler PRIVATE
    Boost::boost
    Boost::json
)

target_compile_options(proxy_scheduler PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)