# Wayland Minimums

A standalone, zero-framework **C++20** reference implementation establishing the baseline architecture required to build native, hardware-accelerated applications directly on **Wayland**.

This project provides a complete, low-level foundation for any GPU-accelerated application—including custom UI toolkits, digital painting and CAD software, scientific visualizers, game engines, and kiosk applications—without relying on third-party windowing abstractions (such as GLFW, SDL, GTK, or Qt).

---

## Architecture Overview

```
                                 ┌────────────────────────────────────────┐
                                 │       Wayland Compositor (Server)      │
                                 │        (Hyprland, Sway, GNOME)         │
                                 └───────────┬───────────────┬────────────┘
                                             │               │
                             Wayland IPC     │               │  Wayland Extensions
                        (wayland-client)     │               │  (xdg-shell, tablet-v2)
                                             ▼               ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                Application Architecture                                │
│                                                                                        │
│  ┌─────────────────────────┐  ┌─────────────────────────┐  ┌────────────────────────┐  │
│  │   Surface & Windowing   │  │    OpenGL Render Loop   │  │   Hardware Handlers    │  │
│  │  • wl_compositor        │  │  • EGLDisplay / Context │  │  • Mouse (wl_pointer)  │  │
│  │  • wl_surface           │  │  • wl_egl_window bridge │  │  • Keys (wl_kbd + XKB) │  │
│  │  • xdg_wm_base          │  │  • OpenGL 3.3 Core      │  │  • Touch (wl_touch)    │  │
│  │  • xdg_toplevel         │  │  • Shaders & VAO/VBO    │  │  • Stylus (tablet-v2)  │  │
│  └─────────────────────────┘  └─────────────────────────┘  └───────────┬────────────┘  │
│                                                                        │               │
│                               ┌────────────────────────────────────────┴────────────┐  │
│                               │           Input Mapper (input_mapper.h)             │  │
│                               │  • Virtual Actions (Jump, Move, Attack, Quit)       │  │
│                               │  • Virtual Axes (MoveX/Y, LookX/Y, Triggers)        │  │
│                               │  • Deadzones, Inversion, Multi-Device Mapping       │  │
│                               └────────────────────────┬────────────────────────────┘  │
└────────────────────────────────────────────────────────┼───────────────────────────────┘
                                                         │ Kernel evdev
                                                         ▼
                                             ┌────────────────────────┐
                                             │ Linux Direct Input     │
                                             │ • Gamepads (/dev/input)│
                                             └────────────────────────┘
```

---

## Key Subsystems

### 1. Windowing & Display Lifecycle (`xdg-shell`)
Core Wayland only knows about raw rectangular buffers (`wl_surface`). Windowing semantics—such as fullscreen negotiation, geometry configuration, state handshakes (`xdg_surface_ack_configure`), and compositor keep-alive heartbeats (`xdg_wm_base_pong`)—are implemented directly against the stable XDG Shell protocol.

### 2. Hardware-Accelerated Graphics (`EGL` + `OpenGL Core`)
- Connects EGL directly to the Wayland display (`eglGetDisplay((EGLNativeDisplayType)wl_disp)`).
- Wraps the Wayland surface in a native `wl_egl_window` to handle dynamic buffer resizing and synchronization.
- Compiles and links programmable vertex and fragment shaders using OpenGL 3.3 Core Profile.

### 3. Non-Blocking Event Loop (`poll`)
Uses the canonical Wayland non-blocking read pattern:
```cpp
wl_display_flush(wl_disp);
while (wl_display_prepare_read(wl_disp) != 0) {
    wl_display_dispatch_pending(wl_disp);
}
struct pollfd pfd = { wl_display_get_fd(wl_disp), POLLIN, 0 };
if (poll(&pfd, 1, 0) > 0) {
    wl_display_read_events(wl_disp);
} else {
    wl_display_cancel_read(wl_disp);
}
wl_display_dispatch_pending(wl_disp);
```
This guarantees that compositor messages are dispatched immediately without blocking, allowing GPU rendering and swap-buffers to run at native display refresh rates.

### 4. Input Architecture

| Peripheral | Protocol / Interface | Implementation Details |
| :--- | :--- | :--- |
| **Mouse / Pointer** | `wl_pointer` | Motion tracking, surface enter/leave, multi-button states, discrete & continuous scroll wheels. |
| **Keyboard** | `wl_keyboard` + `libxkbcommon` | Memory-maps (`mmap`) the shared-memory keymap file descriptor (`memfd`) sent by the compositor; tracks shift/alt/ctrl modifier states and translates scancodes into keysyms. |
| **Multi-Touch** | `wl_touch` | Simultaneous contact points, unique finger ID lifecycle tracking (`down`, `up`, `motion`), touch cancellation, and major/minor ellipse shape reports. |
| **Graphics Tablet & Stylus** | `zwp_tablet_manager_v2` (`tablet-v2`) | Full support for digitizer pens and drawing tablets: surface proximity tracking, tip contact, normalized pressure (`0`–`65535`), 2D tilt angles ($X/Y$ in degrees), and barrel buttons. |
| **Game Controllers** | Linux Kernel (`/dev/input/js*`) | Gamepads intentionally bypass Wayland entirely. Handled through direct non-blocking kernel polling, featuring hotplug reconnection and analog deadzone filtering. |

### 5. Input Mapping & Virtual Action Abstraction (`input_mapper.h`)
Instead of hardcoding raw device scancodes throughout game or application code, all input peripherals feed into a centralized `InputMapper`:
- **Digital Actions**: Maps keysyms, mouse buttons, and gamepad buttons to abstract actions (`Action::Jump`, `Action::PrimaryAction`, `Action::Quit`).
- **Analog Virtual Axes**: Maps analog sticks and triggers to normalized axes (`VirtualAxis::MoveX`, `LookY`, etc.) with customizable per-axis deadzones and sign inversion.
- **Frame State Lifecycle**: Tracks both persistent holds (`is_action_down()`) and single-frame triggers (`is_action_just_pressed()`), cleared each frame with `post_frame_update()`.

---

## How to Include This in Your Own Project

If you are building your own engine, tool, or windowing backend and want to use this codebase, here is the exact procedure:

### 1. Files to Copy
Copy the following files into your project source tree:
- `xdg-shell-client-protocol.h` & `xdg-shell-protocol.c` (Desktop windowing protocol)
- `tablet-v2-client-protocol.h` & `tablet-v2-protocol.c` (Stylus / tablet protocol)
- `input_mapper.h` (Hardware-to-virtual input remapping engine)
- The window setup, EGL lifecycle, and poll loop from `main.cpp` (or wrap them in your own `Window` class).

> [!NOTE]
> If you prefer generating the protocol files fresh from your system's XML definitions using `wayland-scanner`:
> ```bash
> # XDG Shell
> wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
> wayland-scanner private-code  /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
>
> # Tablet V2
> wayland-scanner client-header /usr/share/wayland-protocols/unstable/tablet/tablet-unstable-v2.xml tablet-v2-client-protocol.h
> wayland-scanner private-code  /usr/share/wayland-protocols/unstable/tablet/tablet-unstable-v2.xml tablet-v2-protocol.c
> ```

### 2. Crucial Compilation Rule: C vs C++ Linkage
**Always compile `xdg-shell-protocol.c` and `tablet-v2-protocol.c` with a C compiler (e.g. `gcc` or C language mode in CMake), NOT with a C++ compiler.**

In C, `const struct wl_interface` symbols have external linkage by default. In C++, `const` global objects have internal linkage by default. Compiling these generated files with `g++` without special macro overrides will result in duplicate symbols or missing external symbol link errors.

### 3. Integrating with CMake
Add the following to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_wayland_app C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_DEPS REQUIRED 
    wayland-client 
    wayland-egl 
    egl 
    gl 
    xkbcommon
)

add_executable(my_wayland_app
    src/main.cpp
    src/xdg-shell-protocol.c
    src/tablet-v2-protocol.c
)

target_include_directories(my_wayland_app PRIVATE 
    ${WAYLAND_DEPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(my_wayland_app PRIVATE 
    ${WAYLAND_DEPS_LIBRARIES}
)
```

### 4. Integrating with a Custom Makefile
```makefile
CXX = g++
CC = gcc
CXXFLAGS = -std=c++20 -O2 -Wall
CFLAGS = -O2 -Wall
LIBS = $(shell pkg-config --libs wayland-client wayland-egl egl gl xkbcommon)

my_app: main.o xdg-shell-protocol.o tablet-v2-protocol.o
	$(CXX) $^ -o $@ $(LIBS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

xdg-shell-protocol.o: xdg-shell-protocol.c
	$(CC) $(CFLAGS) -c $< -o $@

tablet-v2-protocol.o: tablet-v2-protocol.c
	$(CC) $(CFLAGS) -c $< -o $@
```

### 5. Architectural Lifecycle to Implement in Code
When writing your application around this foundation, follow this initialization sequence:

1. **Connect & Bind Registry**:
   - Call `wl_display_connect(nullptr)`.
   - Obtain `wl_registry` and bind the `wl_compositor`, `xdg_wm_base`, `wl_seat`, and optionally `zwp_tablet_manager_v2`.
   - Call `wl_display_roundtrip(wl_disp)`.
2. **Create Surface & Configure Handshake**:
   - Create `wl_surface` from compositor.
   - Create `xdg_surface` and `xdg_toplevel`.
   - Register listeners (`xdg_surface_listener` must respond with `xdg_surface_ack_configure`).
   - Call `wl_surface_commit(wl_surf)` and roundtrip so the compositor sends initial configure.
3. **Initialize EGL**:
   - `eglGetDisplay((EGLNativeDisplayType)wl_disp)`.
   - Choose configuration (`EGL_SURFACE_TYPE, EGL_WINDOW_BIT`, `EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT`).
   - Create native window bridge: `wl_egl_win = wl_egl_window_create(wl_surf, width, height)`.
   - Create EGL window surface: `eglCreateWindowSurface(..., (EGLNativeWindowType)wl_egl_win, ...)`.
   - `eglMakeCurrent()`.
4. **Feed Inputs to `InputMapper`**:
   - Pointer button -> `input_mapper.process_digital(InputDevice::Mouse, button, pressed);`
   - Keyboard key -> Translate scancode via `libxkbcommon`, then `input_mapper.process_digital(InputDevice::Keyboard, keysym, pressed);`
   - Gamepad axes/buttons -> Read `/dev/input/js*` and pass to `input_mapper.process_axis` / `process_digital`.
5. **Frame Loop**:
   - Flush Wayland events, prepare read, poll display FD, dispatch pending events.
   - Query input states via `input_mapper.is_action_down(Action::...)` or `input_mapper.get_axis(VirtualAxis::...)`.
   - Render your OpenGL / Vulkan scene.
   - Swap buffers: `eglSwapBuffers(egl_disp, egl_surface)`.
   - Call `input_mapper.post_frame_update()` to reset single-frame triggers.

---

## Dependencies

On Arch Linux / modern distributions:
```bash
sudo pacman -S wayland wayland-protocols libxkbcommon mesa gcc cmake pkgconf
```

Libraries linked:
- `libwayland-client` — Wayland IPC client library
- `libwayland-egl` — EGL window surface bridge
- `libEGL` — EGL context and surface manager
- `libGL` — OpenGL graphics runtime
- `libxkbcommon` — Keyboard keymap compilation and translation

---

## Building and Running

Three build methods are provided and maintained:

### Method 1: Direct Script (`build.sh`)
Executes a direct shell build script with full flag documentation:
```bash
./build.sh
```

### Method 2: CMake (`BUILDS/`)
Builds out-of-source into the `BUILDS/` directory:
```bash
cmake -B BUILDS -S . && cmake --build BUILDS
```

### Method 3: Makefile
```bash
make -j$(nproc)
```

### Running
```bash
./full_gl
```

---

## Interactive Controls & Visual Verification

The rendered triangle dynamically changes color to provide real-time visual confirmation across all input devices:

- **Keyboard**:
  - `Escape`: Clean application shutdown and resource deallocation.
  - `Space`: Reset triangle to default orange.
  - `R` / `G` / `B`: Change triangle color to Red, Green, or Blue.
- **Mouse**:
  - `Left Click`: Green
  - `Right Click`: Blue
  - `Middle Click`: Yellow
- **Touch**:
  - `Single Tap`: Triggered when releasing without moving; turns triangle lime green.
  - `Long Press`: Stationary hold for >500ms; turns triangle gold.
  - `1-Finger Drag`: Continuous drag delta streaming.
  - `2-Finger Pinch`: Real-time pinch zoom in/out with scaling factor and span distance.
  - `2-Finger Pan`: Midpoint two-finger scroll/pan tracking.
- **Graphics Tablet & Stylus**:
  - `Proximity Enter / Leave`: Hover detection with height distance percentage.
  - `Drawing Stroke`: Tip down/up lifecycle; turns triangle turquoise.
  - `Pressure Sensitivity`: 0–65535 normalized pressure with ASCII gauge bar (`[#######---] 70%`), dynamically scaling color brightness.
  - `2D Tilt`: Real-time pen tilt angle (X/Y degrees).
  - `Barrel Buttons`: Primary and secondary barrel button shortcuts; turns triangle purple.
- **Gamepad / Controller**:
  - Face and shoulder buttons dynamically cycle procedural colors.
