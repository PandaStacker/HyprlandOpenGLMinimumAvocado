# Wayland Minimums

A standalone, zero-framework **C++26** reference implementation establishing the baseline architecture required to build native, hardware-accelerated applications directly on **Wayland**.

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
│  │   Surface & Windowing   │  │    OpenGL Render Loop   │  │   Input Architecture   │  │
│  │  • wl_compositor        │  │  • EGLDisplay / Context │  │  • Mouse (wl_pointer)  │  │
│  │  • wl_surface           │  │  • wl_egl_window bridge │  │  • Keys (wl_kbd + XKB) │  │
│  │  • xdg_wm_base          │  │  • OpenGL 3.3 Core      │  │  • Touch (wl_touch)    │  │
│  │  • xdg_toplevel         │  │  • Shaders & VAO/VBO    │  │  • Stylus (tablet-v2)  │  │
│  └─────────────────────────┘  └─────────────────────────┘  └───────────┬────────────┘  │
└────────────────────────────────────────────────────────────────────────┼───────────────┘
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

Two build methods are provided and maintained:

### Method 1: Direct Script (`build.sh`)
Executes a single, clean `g++` command with full flag documentation:
```bash
./build.sh
```

### Method 2: CMake (`BUILDS/`)
Builds out-of-source into the `BUILDS/` directory:
```bash
cmake -B BUILDS -S . && cmake --build BUILDS
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
