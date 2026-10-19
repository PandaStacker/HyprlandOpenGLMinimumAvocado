#!/bin/bash
# AI SHOULD NEVER RUN THIS FILE. It may edit it and keep it up to date.
# ---------------------------------------------------------------------------------------------
# Zero dependencies needed: Uses pure Bash and standard terminal ANSI/Unicode escape sequences.
# ---------------------------------------------------------------------------------------------
set -e

mkdir -p BUILDS

TOTAL_STEPS=8
CURRENT_STEP=0

# Colors & Formatting
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
RESET="\033[0m"

draw_progress() {
    local step=$1
    local total=$2
    local label=$3
    local pct=$(( step * 100 / total ))
    local bar_width=30
    local filled=$(( pct * bar_width / 100 ))
    local empty=$(( bar_width - filled ))

    local bar=""
    for ((i=0; i<filled; i++)); do bar+="█"; done
    for ((i=0; i<empty; i++)); do bar+="░"; done

    printf "\r${BLUE}[%3d%%]${RESET} ${GREEN}[%s]${RESET} ${BOLD}%-42s${RESET}" "$pct" "$bar" "$label"
}

run_step() {
    local label=$1
    shift
    draw_progress "$CURRENT_STEP" "$TOTAL_STEPS" "$label"
    "$@"
    CURRENT_STEP=$((CURRENT_STEP + 1))
    draw_progress "$CURRENT_STEP" "$TOTAL_STEPS" "$label"
}

echo -e "${BOLD}Building Wayland Minimums...${RESET}"

run_step "Compiling xdg-shell protocol..." \
    gcc -c xdg-shell-protocol.c -o BUILDS/xdg-shell-protocol.o

run_step "Compiling tablet-v2 protocol..." \
    gcc -c tablet-v2-protocol.c -o BUILDS/tablet-v2-protocol.o

run_step "Compiling wayland_window.cpp..." \
    g++ -std=c++20 -c wayland_window.cpp -o BUILDS/wayland_window.o

run_step "Compiling shader.cpp..." \
    g++ -std=c++20 -c shader.cpp -o BUILDS/shader.o

run_step "Compiling graphics.cpp..." \
    g++ -std=c++20 -c graphics.cpp -o BUILDS/graphics.o

run_step "Compiling app.cpp..." \
    g++ -std=c++20 -c app.cpp -o BUILDS/app.o

run_step "Packaging libwayland_minimums.a..." \
    ar rcs BUILDS/libwayland_minimums.a BUILDS/wayland_window.o BUILDS/shader.o BUILDS/graphics.o BUILDS/app.o BUILDS/xdg-shell-protocol.o BUILDS/tablet-v2-protocol.o

run_step "Linking full_gl executable..." \
    g++ -std=c++20 main.cpp -LBUILDS -lwayland_minimums \
        -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon -o full_gl

echo -e "\n${GREEN}✔ Build successful! Output: ./full_gl and BUILDS/libwayland_minimums.a${RESET}\n"
