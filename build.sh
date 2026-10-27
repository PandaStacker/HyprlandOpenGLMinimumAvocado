#!/bin/bash
# ---------------------------------------------------------------------------------------------
# Zero dependencies needed: Uses pure Bash and standard terminal ANSI/Unicode escape sequences.
# ---------------------------------------------------------------------------------------------
set -e

source "$(dirname "$0")/loading_bar.sh"

mkdir -p BUILDS

# Check if target is missing or if any dependency is newer than the target
needs_rebuild() {
    local target="$1"
    shift
    if [ ! -f "$target" ]; then
        return 0
    fi
    for dep in "$@"; do
        if [ -f "$dep" ] && [ "$dep" -nt "$target" ]; then
            return 0
        fi
    done
    return 1
}

# Executes the compilation command only if the target needs rebuilding
compile_if_needed() {
    local target="$1"
    shift
    local deps=()
    while [ "$1" != "--" ]; do
        deps+=("$1")
        shift
    done
    shift # discard '--'

    if needs_rebuild "$target" "${deps[@]}"; then
        "$@"
    fi
}

# Compile shaders only when modified
compile_shaders_if_needed() {
    if needs_rebuild shaders/triangle.vert.spv shaders/triangle.vert; then
        glslc shaders/triangle.vert -o shaders/triangle.vert.spv || return $?
    fi
    if needs_rebuild shaders/triangle.frag.spv shaders/triangle.frag; then
        glslc shaders/triangle.frag -o shaders/triangle.frag.spv || return $?
    fi
    return 0
}

PCH_SRC="include/pch.hpp"
PCH_GCH="BUILDS/pch.hpp.gch"
PCH_FLAGS=(-IBUILDS -include pch.hpp)

LIB_OBJS=(
    BUILDS/wayland_window.o
    BUILDS/shader.o
    BUILDS/graphics.o
    BUILDS/app.o
    BUILDS/client.o
    BUILDS/server.o
    BUILDS/user_controller.o
    BUILDS/xdg-shell-protocol.o
    BUILDS/tablet-v2-protocol.o
    BUILDS/viewporter-protocol.o
    BUILDS/fractional-scale-v1-protocol.o
    BUILDS/tearing-control-v1-protocol.o
)

echo -e "${BOLD}Building Wayland Minimums...${RESET}"

run_step "Precompiling headers (PCH)..." \
    compile_if_needed "$PCH_GCH" "$PCH_SRC" -- \
        g++ -std=c++20 -Iinclude -x c++-header "$PCH_SRC" -o "$PCH_GCH"

run_step "Compiling xdg-shell protocol..." \
    compile_if_needed BUILDS/xdg-shell-protocol.o \
        protocols/xdg-shell-protocol.c protocols/xdg-shell-client-protocol.h -- \
        gcc -c protocols/xdg-shell-protocol.c -o BUILDS/xdg-shell-protocol.o

run_step "Compiling tablet-v2 protocol..." \
    compile_if_needed BUILDS/tablet-v2-protocol.o \
        protocols/tablet-v2-protocol.c protocols/tablet-v2-client-protocol.h -- \
        gcc -c protocols/tablet-v2-protocol.c -o BUILDS/tablet-v2-protocol.o

run_step "Compiling viewporter protocol..." \
    compile_if_needed BUILDS/viewporter-protocol.o \
        protocols/viewporter-protocol.c protocols/viewporter-client-protocol.h -- \
        gcc -c protocols/viewporter-protocol.c -o BUILDS/viewporter-protocol.o

run_step "Compiling fractional-scale-v1 protocol..." \
    compile_if_needed BUILDS/fractional-scale-v1-protocol.o \
        protocols/fractional-scale-v1-protocol.c protocols/fractional-scale-v1-client-protocol.h -- \
        gcc -c protocols/fractional-scale-v1-protocol.c -o BUILDS/fractional-scale-v1-protocol.o

run_step "Compiling tearing-control-v1 protocol..." \
    compile_if_needed BUILDS/tearing-control-v1-protocol.o \
        protocols/tearing-control-v1-protocol.c protocols/tearing-control-v1-client-protocol.h -- \
        gcc -c protocols/tearing-control-v1-protocol.c -o BUILDS/tearing-control-v1-protocol.o

run_step "Compiling wayland_window.cpp..." \
    compile_if_needed BUILDS/wayland_window.o \
        wayland_window.cpp wayland_window.hpp input_mapper.hpp \
        protocols/xdg-shell-client-protocol.h protocols/tablet-v2-client-protocol.h \
        protocols/viewporter-client-protocol.h protocols/fractional-scale-v1-client-protocol.h \
        protocols/tearing-control-v1-client-protocol.h "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c wayland_window.cpp -o BUILDS/wayland_window.o

run_step "Compiling GLSL shaders to SPIR-V..." \
    compile_shaders_if_needed

run_step "Compiling shader.cpp..." \
    compile_if_needed BUILDS/shader.o \
        shader.cpp shader.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c shader.cpp -o BUILDS/shader.o

run_step "Compiling graphics.cpp..." \
    compile_if_needed BUILDS/graphics.o \
        graphics.cpp graphics.hpp shader.hpp client.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c graphics.cpp -o BUILDS/graphics.o

run_step "Compiling user_controller.cpp..." \
    compile_if_needed BUILDS/user_controller.o \
        user_controller.cpp user_controller.hpp input_mapper.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c user_controller.cpp -o BUILDS/user_controller.o

run_step "Compiling client.cpp..." \
    compile_if_needed BUILDS/client.o \
        client.cpp client.hpp input_mapper.hpp user_controller.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c client.cpp -o BUILDS/client.o

run_step "Compiling server.cpp..." \
    compile_if_needed BUILDS/server.o \
        server.cpp server.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c server.cpp -o BUILDS/server.o

run_step "Compiling app.cpp..." \
    compile_if_needed BUILDS/app.o \
        app.cpp app.hpp wayland_window.hpp graphics.hpp client.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c app.cpp -o BUILDS/app.o

run_step "Packaging libwayland_minimums.a..." \
    compile_if_needed BUILDS/libwayland_minimums.a "${LIB_OBJS[@]}" -- \
        ar rcs BUILDS/libwayland_minimums.a "${LIB_OBJS[@]}"

run_step "Compiling main.cpp..." \
    compile_if_needed BUILDS/main.o \
        main.cpp app.hpp client.hpp "$PCH_GCH" -- \
        g++ -std=c++20 "${PCH_FLAGS[@]}" -Iinclude -c main.cpp -o BUILDS/main.o

run_step "Linking wayland_app executable..." \
    compile_if_needed wayland_app \
        BUILDS/main.o BUILDS/libwayland_minimums.a -- \
        g++ -std=c++20 -fuse-ld=gold BUILDS/main.o -LBUILDS -lwayland_minimums \
            -lwayland-client -lvulkan -lxkbcommon \
            -o wayland_app

echo -e "\n${GREEN}✔ Build successful! Output: ./wayland_app and BUILDS/libwayland_minimums.a${RESET}\n"
