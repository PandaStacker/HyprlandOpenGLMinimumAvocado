#!/bin/bash
# ---------------------------------------------------------------------------------------------
# Compilation Command Breakdown:
# -std=c++26:
#     Enables the modern C++26 language standard.
#
# main.cpp:
#     The main C++ application code.
#
# -x c xdg-shell-protocol.c:
#     Forces g++ to compile this generated file as C rather than C++. In C, const variables
#     have external linkage by default, whereas in C++ they default to internal (static)
#     linkage. Compiling as C ensures symbols like xdg_wm_base_interface can be linked.
#
# -lwayland-client:
#     Core Wayland client library for display connection, registry, seat, and surfaces.
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
#     Parses the XKB keymap string provided by Wayland in an anonymous shared memory fd
#     to convert raw hardware scancodes into keysyms (e.g. Escape, Space).
#
# -o full_gl:
#     Output executable binary name.
# ---------------------------------------------------------------------------------------------
g++ -std=c++26 main.cpp -x c xdg-shell-protocol.c -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon -o full_gl
