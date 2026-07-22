cmake_minimum_required(VERSION 3.20)

project(proxy_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS json)

add_library(control_plane_core
    src/node_registry.cpp
    src/scheduler.cpp
    src/api/handlers.cpp
    src/api/serialization.cpp
    src/api/validation.cpp
)

target_include_directories(control_plane_core PUBLIC
    include
)

target_link_libraries(control_plane_core PUBLIC
    Boost::boost
    Boost::json
)

target_compile_options(control_plane_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(proxy_scheduler
    src/main.cpp
    src/http_server.cpp
    src/health_checker.cpp
)

target_link_libraries(proxy_scheduler PRIVATE control_plane_core)
target_compile_options(proxy_scheduler PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(phase1_tests tests/phase1_tests.cpp)
    target_link_libraries(phase1_tests PRIVATE control_plane_core)
    target_compile_options(phase1_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase1_tests COMMAND phase1_tests)
endif()
