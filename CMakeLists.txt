cmake_minimum_required(VERSION 3.20)
project(wayland_minimums C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED wayland-client wayland-egl egl gl xkbcommon)

add_executable(full_gl main.cpp xdg-shell-protocol.c)
target_include_directories(full_gl PRIVATE ${DEPS_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(full_gl PRIVATE ${DEPS_LIBRARIES})
