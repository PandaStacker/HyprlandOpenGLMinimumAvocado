# Wayland Minimums

A standalone, zero-framework **C++20** reference implementation establishing the baseline architecture required to build native, hardware-accelerated applications directly on **Wayland**.

This project provides a complete, low-level foundation for any GPU-accelerated application—including custom UI toolkits, digital painting and CAD software, scientific visualizers, game engines, and kiosk applications—without relying on third-party windowing abstractions (such as GLFW, SDL, GTK, or Qt).

This is much closer to how I envisioned app development, it's easy to understand. The diagram below is not what I'm talking about, will provide.

Once we add in a bunch of OpenGL helpers and basic shaders, I think this will basically be there. The networking will be another repo.

Alright, we've got something much closer to the right solution:
The app.cpp just manages the arguments passed in and then the overall loop, window polling, local_sim updating, and the graphics.
The local_sim.cpp manages local control and changes while receiving the world_sim. It receives the world sim, processes local inputs and the local controller, simulates those local inputs in the world_sim, then sends updates to the world_sim. It also keeps track of the user controller and its local context. It has a network manager it uses to send and receive networking data. It receives, processes, sends. It also keeps track of whether or not a menu is open on top of everything.
The world_sim.cpp processes inputs from the outside, such as input changes, then simulates and validates whether or not this was possible, comparing against its history. It issues corrections where it sees fit. Then, it updates its history. Then it sends out the new world changes to all who need it. If offline, then validation still needs to happen here, but we can essentially merge local and world sim into one order without the networking steps: process local controller -> simulate and validate tick -> update history -> repeat. This can be done with a very simple branch of some kind.
The user controller in the local sim has the mappings from the hardware pollers to the actions and from the actions, and then the actions to actual behaviour.

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

## Quick Start: Using WaylandWindow in Your Code

Instantiate `WaylandWindow`, focus on your OpenGL code, and let the library handle the entire Wayland/EGL/input lifecycle:

```cpp
#include "wayland_window.h"

int main() {
    // 1. Create window and hardware-accelerated OpenGL context
    WaylandWindow window(1920, 1080, "My OpenGL App", /*fullscreen=*/false);

    // Optional: Custom input remapping
    // Bind keyboard 'F' key and Gamepad button 2 (X) to trigger Action::SecondaryAction
    // window.input().bind_action(Action::SecondaryAction, InputDevice::Keyboard, XKB_KEY_f);
    // window.input().bind_action(Action::SecondaryAction, InputDevice::Gamepad, 2);

    // 2. Setup your OpenGL shaders and meshes
    // ... glGenVertexArrays, glUseProgram, etc. ...

    // 3. Render loop
    while (window.is_running()) {
        window.poll_events();

        // Check input actions
        if (window.input().is_action_down(Action::PrimaryAction)) {
            // Left click / Gamepad A
        }

        // if (window.input().is_action_just_pressed(Action::SecondaryAction)) {
        //     // Triggered by Right Click, Stylus Barrel, or custom mapped 'F' key!
        // }

        // Render with OpenGL
        glClear(GL_COLOR_BUFFER_BIT);
        // ... draw calls ...

        window.swap_buffers();
    }
    return 0;
}
```

---

## How to Include This in Your Own Project

You can use this project directly as a static library or drop the files into your build system without dealing with C protocol fiddling:

### Option A: Using CMake (`add_subdirectory`)
Add this repository to your project (as a submodule or subdirectory):

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_game CXX)

set(CMAKE_CXX_STANDARD 20)

# Add Wayland-Minimums as a subdirectory
add_subdirectory(path/to/Wayland-Minimums)

# Your executable
add_executable(my_game src/main.cpp)

# Link wayland_minimums — dependencies and include paths are propagated automatically!
target_link_libraries(my_game PRIVATE wayland_minimums)
```

### Option B: Using the Precompiled Static Library (`libwayland_minimums.a`)
1. Build the library once:
   ```bash
   make lib
   ```
2. Copy `wayland_window.h`, `input_mapper.h`, and `BUILDS/libwayland_minimums.a` into your project.
3. Compile and link your C++ code with zero C protocol setup:
   ```bash
   g++ -std=c++20 main.cpp -L. -lwayland_minimums \
       $(pkg-config --libs wayland-client wayland-egl egl gl xkbcommon) -o my_game
   ```

### Option C: Adding Files to an Existing Build System
If compiling directly within your own tree:
1. Copy:
   - `wayland_window.h` & `wayland_window.cpp`
   - `input_mapper.h`
   - `xdg-shell-client-protocol.h` & `xdg-shell-protocol.c`
   - `tablet-v2-client-protocol.h` & `tablet-v2-protocol.c`
2. Ensure `xdg-shell-protocol.c` and `tablet-v2-protocol.c` are compiled with `gcc` (or in C mode), and `wayland_window.cpp` with `g++ -std=c++20`.
3. Link with `-lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon`.

---

## WaylandWindow API Summary

| Method | Description |
| :--- | :--- |
| `WaylandWindow(width, height, title, fullscreen)` | Constructs window and creates hardware-accelerated OpenGL context. |
| `window.is_running()` | Returns `true` while the window is active; `false` on exit or compositor close. |
| `window.close()` | Requests window and application termination. |
| `window.poll_events()` | Non-blocking dispatch of Wayland IPC, touch gestures, and kernel gamepad events. |
| `window.swap_buffers()` | Flips front/back OpenGL buffers via EGL. |
| `window.set_vsync(bool)` | Toggles vertical synchronization (swap interval 1 vs 0) at runtime. |
| `window.input()` | Returns reference to [`InputMapper`](file:///home/olive/.files/.code/C++/Wayland-Minimums/input_mapper.h) for digital actions and analog axes. |
| `window.set_fullscreen(bool)` | Dynamically toggles fullscreen via XDG Shell. |
| `window.on_resize(cb)` | Optional callback invoked when compositor resizes window. |
| `window.on_mouse_button(cb)` | Optional callback for raw mouse button events. |
| `window.on_mouse_motion(cb)` | Optional callback for raw cursor motion. |
| `window.on_key(cb)` | Optional callback for raw keyboard scancodes and keysyms. |
| `window.on_touch(cb)` | Optional callback for raw multi-touch down/motion/up events. |
| `window.on_stylus(cb)` | Optional callback for graphics tablet proximity, pressure, and tilt. |
| `window.get_wl_display()` | Returns raw `wl_display*` pointer for Vulkan / custom integrations. |
| `window.get_wl_surface()` | Returns raw `wl_surface*` pointer. |
| `window.get_egl_context()` | Returns raw `EGLContext`. |

---

## Configuring OpenGL Capabilities (Zero EGL Exposure)

You can configure hardware framebuffer and context settings directly through [`WindowConfig`](file:///home/olive/.files/.code/C++/Wayland-Minimums/wayland_window.h#L14-L33) using standard C++ types without touching EGL headers or linking issues:

```cpp
WaylandWindow window(WindowConfig{
    .width        = 1920,
    .height       = 1080,
    .title        = "My 3D Game",
    .fullscreen   = false,

    // Hardware OpenGL context & framebuffer configuration:
    .gl_major     = 3,     // OpenGL 3.3 Core Profile
    .gl_minor     = 3,
    .gl_core_profile = true,
    .depth_bits   = 24,    // 24-bit depth buffer for 3D depth-testing
    .stencil_bits = 8,     // 8-bit stencil buffer for shadows/masks
    .samples      = 4,     // 4x MSAA hardware anti-aliasing (falls back automatically if unsupported)
    .vsync        = true   // Vertical synchronization to display refresh rate
});

// VSync can also be toggled dynamically at runtime:
window.set_vsync(false); // Uncap framerate
```

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
Executes a direct shell build script featuring a zero-dependency in-terminal progress bar:
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
# Run application (exit button '_' is disabled by default for consumer safety)
./full_gl

# Run application with the '_' exit button enabled for interactive testing:
./full_gl --enable-exit
# or:
./full_gl -e
```

---

## Interactive Controls & Visual Verification

The rendered triangle dynamically changes color to provide real-time visual confirmation across all input devices:

- **Keyboard**:
  - `_` (Underscore): Application exit button. **Disabled by default** so that consumer projects incorporating these files do not exit unexpectedly when typing or binding keys. Enable with `--enable-exit` / `-e` at startup or in code via `input_mapper.enable_exit_action(true)`.
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
