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

run_step "Compiling wayland_window.cpp..." \
    g++ -std=c++20 -c wayland_window.cpp -o BUILDS/wayland_window.o

run_step "Compiling shader.cpp..." \
    g++ -std=c++20 -c shader.cpp -o BUILDS/shader.o

run_step "Compiling graphics.cpp..." \
    g++ -std=c++20 -c graphics.cpp -o BUILDS/graphics.o

run_step "Compiling user_controller.cpp..." \
    g++ -std=c++20 -c user_controller.cpp -o BUILDS/user_controller.o

run_step "Compiling client.cpp..." \
    g++ -std=c++20 -c client.cpp -o BUILDS/client.o

run_step "Compiling server.cpp..." \
    g++ -std=c++20 -c server.cpp -o BUILDS/server.o

run_step "Compiling app.cpp..." \
    g++ -std=c++20 -c app.cpp -o BUILDS/app.o

run_step "Packaging libwayland_minimums.a..." \
    ar rcs BUILDS/libwayland_minimums.a BUILDS/wayland_window.o BUILDS/shader.o BUILDS/graphics.o BUILDS/app.o BUILDS/client.o BUILDS/server.o BUILDS/user_controller.o BUILDS/xdg-shell-protocol.o BUILDS/tablet-v2-protocol.o

run_step "Linking full_gl executable..." \
    g++ -std=c++20 main.cpp -LBUILDS -lwayland_minimums \
        -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon \
        -o wayland_app

echo -e "\n${GREEN}✔ Build successful! Output: ./full_gl and BUILDS/libwayland_minimums.a${RESET}\n"
