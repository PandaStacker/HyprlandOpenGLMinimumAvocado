#!/bin/bash
# ---------------------------------------------------------------------------------------------
# Zero dependencies needed: Uses pure Bash and standard terminal ANSI/Unicode escape sequences.
# ---------------------------------------------------------------------------------------------
set -e

source "$(dirname "$0")/loading_bar.sh"

mkdir -p BUILDS

echo -e "${BOLD}Building Wayland Minimums...${RESET}"

run_step "Compiling xdg-shell protocol..." \
    gcc -c protocols/xdg-shell-protocol.c -o BUILDS/xdg-shell-protocol.o

run_step "Compiling tablet-v2 protocol..." \
    gcc -c protocols/tablet-v2-protocol.c -o BUILDS/tablet-v2-protocol.o

run_step "Compiling viewporter protocol..." \
    gcc -c protocols/viewporter-protocol.c -o BUILDS/viewporter-protocol.o

run_step "Compiling fractional-scale-v1 protocol..." \
    gcc -c protocols/fractional-scale-v1-protocol.c -o BUILDS/fractional-scale-v1-protocol.o

run_step "Compiling tearing-control-v1 protocol..." \
    gcc -c protocols/tearing-control-v1-protocol.c -o BUILDS/tearing-control-v1-protocol.o

run_step "Compiling wayland_window.cpp..." \
    g++ -std=c++20 -c wayland_window.cpp -o BUILDS/wayland_window.o

run_step "Compiling GLSL shaders to SPIR-V..." \
    glslc shaders/triangle.vert -o shaders/triangle.vert.spv && glslc shaders/triangle.frag -o shaders/triangle.frag.spv

run_step "Compiling shader.cpp..." \
    g++ -std=c++20 -Iinclude -c shader.cpp -o BUILDS/shader.o

run_step "Compiling graphics.cpp..." \
    g++ -std=c++20 -Iinclude -c graphics.cpp -o BUILDS/graphics.o

run_step "Compiling user_controller.cpp..." \
    g++ -std=c++20 -Iinclude -c user_controller.cpp -o BUILDS/user_controller.o

run_step "Compiling client.cpp..." \
    g++ -std=c++20 -Iinclude -c client.cpp -o BUILDS/client.o

run_step "Compiling server.cpp..." \
    g++ -std=c++20 -Iinclude -c server.cpp -o BUILDS/server.o

run_step "Compiling app.cpp..." \
    g++ -std=c++20 -Iinclude -c app.cpp -o BUILDS/app.o

run_step "Packaging libwayland_minimums.a..." \
    ar rcs BUILDS/libwayland_minimums.a BUILDS/wayland_window.o BUILDS/shader.o BUILDS/graphics.o BUILDS/app.o BUILDS/client.o BUILDS/server.o BUILDS/user_controller.o BUILDS/xdg-shell-protocol.o BUILDS/tablet-v2-protocol.o BUILDS/viewporter-protocol.o BUILDS/fractional-scale-v1-protocol.o BUILDS/tearing-control-v1-protocol.o

run_step "Linking wayland_app executable..." \
    g++ -std=c++20 -Iinclude main.cpp -LBUILDS -lwayland_minimums \
        -lwayland-client -lvulkan -lxkbcommon \
        -o wayland_app

echo -e "\n${GREEN}✔ Build successful! Output: ./wayland_app and BUILDS/libwayland_minimums.a${RESET}\n"
