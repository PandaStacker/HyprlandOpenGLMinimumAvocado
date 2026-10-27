cmake_minimum_required(VERSION 3.20)
project(wayland_minimums C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED wayland-client vulkan xkbcommon)

# Static library for effortless inclusion in consumer projects
add_library(wayland_minimums STATIC
    wayland_window.cpp
    shader.cpp
    graphics.cpp
    app.cpp
    client.cpp
    server.cpp
    user_controller.cpp
    protocols/xdg-shell-protocol.c
    protocols/tablet-v2-protocol.c
)
target_include_directories(wayland_minimums PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DEPS_INCLUDE_DIRS}
)
target_link_libraries(wayland_minimums PUBLIC
    ${DEPS_LIBRARIES}
)

# Demo executable
add_executable(wayland_app main.cpp)
target_link_libraries(wayland_app PRIVATE wayland_minimums)
