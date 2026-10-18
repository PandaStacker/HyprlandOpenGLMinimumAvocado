#!/bin/bash
# AI SHOULD NEVER RUN THIS FILE. It may edit it and keep it up to date.
# ---------------------------------------------------------------------------------------------
# Compilation Command Breakdown:
# -std=c++20:
#     Enables the modern C++20 language standard for main.cpp.
#
# main.cpp:
#     The main C++ application code.
#
# gcc -c xdg-shell-protocol.c tablet-v2-protocol.c:
#     Compiles the generated Wayland protocol files as C rather than C++.
#     In C, const interface symbols have external linkage by default, allowing them to be
#     linked to main.cpp without visibility attribute warnings.
#
# -lwayland-client:
#     Core Wayland client library (display, registry, seat, pointer, keyboard, touch).
#
# -lwayland-egl:
#     Bridges native Wayland surfaces to EGL windows (wl_egl_window_create, resize).
#
# -lEGL:
#     EGL display, config, context, and surface lifecycle management.
#
# -lGL:
#     OpenGL graphics pipeline (shaders, buffers, draw calls).
#
# -lxkbcommon:
#     Parses XKB keymap string provided by Wayland in an anonymous shared memory fd
#     to convert raw hardware scancodes into keysyms (e.g. Escape, Space).
#
set -e
mkdir -p BUILDS
gcc -c xdg-shell-protocol.c -o BUILDS/xdg-shell-protocol.o
gcc -c tablet-v2-protocol.c -o BUILDS/tablet-v2-protocol.o
g++ -std=c++20 main.cpp BUILDS/xdg-shell-protocol.o BUILDS/tablet-v2-protocol.o \
    -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon -o full_gl
