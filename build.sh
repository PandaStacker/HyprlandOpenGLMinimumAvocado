#!/bin/bash
# AI SHOULD NEVER RUN THIS FILE. It may edit it and keep it up to date.
# ---------------------------------------------------------------------------------------------
# Compilation Command Breakdown:
# -std=c++26:
#     Enables the modern C++26 language standard.
#
# main.cpp:
#     The main C++ application code.
#
# -x c xdg-shell-protocol.c -x c tablet-v2-protocol.c:
#     Forces g++ to compile the generated Wayland protocol files as C rather than C++.
#     In C, const interface symbols have external linkage by default, allowing them to be
#     linked to main.cpp.
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
# -o full_gl:
#     Output executable binary name.
# ---------------------------------------------------------------------------------------------
g++ -std=c++26 main.cpp -x c xdg-shell-protocol.c -x c tablet-v2-protocol.c -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon -o full_gl
