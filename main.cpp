// Build with: ./build.sh (by human)
// Or: cmake -B BUILDS -S . && cmake --build BUILDS (or make -C BUILDS)
#define GL_GLEXT_PROTOTYPES
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>
#include <linux/joystick.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>

// Wayland Protocol Extensions
#include "xdg-shell-client-protocol.h"
#include "tablet-v2-client-protocol.h"

// Core Wayland and XDG Shell pointers
struct wl_display*            wl_disp      = nullptr;
struct wl_compositor*         wl_comp      = nullptr;
struct wl_surface*            wl_surf      = nullptr;
struct wl_egl_window*         wl_egl_win   = nullptr;
struct xdg_wm_base*           xdg_wm       = nullptr;
struct xdg_surface*           xdg_surf     = nullptr;
struct xdg_toplevel*          xdg_top      = nullptr;

// Wayland Seat & Input Peripherals
struct wl_seat*               wl_st        = nullptr;
struct wl_pointer*            wl_ptr       = nullptr;
struct wl_keyboard*           wl_kbd       = nullptr;
struct wl_touch*              wl_tch       = nullptr;

// Wayland Tablet V2
struct zwp_tablet_manager_v2* tablet_mgr   = nullptr;
struct zwp_tablet_seat_v2*    tablet_seat  = nullptr;

// XKB Keyboard State
struct xkb_context*           xkb_ctx      = nullptr;
struct xkb_keymap*            xkb_map      = nullptr;
struct xkb_state*             xkb_st       = nullptr;

// Linux Kernel Direct Gamepad
int                           gamepad_fd   = -1;
std::string                   gamepad_name = "None";

// EGL Pipeline
EGLDisplay                    egl_disp     = EGL_NO_DISPLAY;
EGLContext                    egl_ctx      = EGL_NO_CONTEXT;
EGLSurface                    egl_surface  = EGL_NO_SURFACE;

// Application State
bool                          running      = true;
float                         tri_color[3] = { 1.0f, 0.5f, 0.2f }; // Orange default

// Helper: High-resolution timestamp in milliseconds
inline uint32_t get_time_ms() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Helper: Visual ASCII progress gauge
std::string make_gauge(float pct, int width = 10) {
    int filled = (int)(pct / 100.0f * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    std::string bar = "[";
    for (int i = 0; i < filled; ++i) bar += "#";
    for (int i = filled; i < width; ++i) bar += "-";
    bar += "]";
    return bar;
}

// Shaders
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); }\n";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uColor;\n"
    "void main() { FragColor = vec4(uColor, 1.0f); }\n";

// -------------------------------------------------------------
// XDG SHELL (Desktop Window Lifecycle)
// -------------------------------------------------------------
static void xdg_toplevel_configure(void* data, struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states) {
    (void)data; (void)toplevel; (void)states;
    if (width > 0 && height > 0 && wl_egl_win) {
        wl_egl_window_resize(wl_egl_win, width, height, 0, 0);
        glViewport(0, 0, width, height);
    }
}
static void xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) {
    (void)data; (void)toplevel;
    std::cout << "[Window] Close requested by compositor." << std::endl;
    running = false;
}
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
    .configure_bounds = nullptr,
    .wm_capabilities = nullptr,
};

static void xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(surface, serial);
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void wm_base_ping(void* data, struct xdg_wm_base* wm, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

// -------------------------------------------------------------
// MOUSE / POINTER INPUT
// -------------------------------------------------------------
static void pointer_enter(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    std::cout << "[Mouse] Enter window at (" << wl_fixed_to_double(sx) << ", " << wl_fixed_to_double(sy) << ")" << std::endl;
}
static void pointer_leave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    std::cout << "[Mouse] Leave window" << std::endl;
}
static void pointer_motion(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)time;
    std::cout << "[Mouse] Motion: (" << wl_fixed_to_double(sx) << ", " << wl_fixed_to_double(sy) << ")" << std::endl;
}
static void pointer_button(void* data, struct wl_pointer* pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state) {
    (void)data; (void)pointer; (void)serial; (void)time;
    bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    std::string btn_name;
    if (button == BTN_LEFT) btn_name = "Left Button";
    else if (button == BTN_RIGHT) btn_name = "Right Button";
    else if (button == BTN_MIDDLE) btn_name = "Middle Button";
    else btn_name = "Button 0x" + std::to_string(button);

    std::cout << "[Mouse] " << btn_name << " " << (pressed ? "PRESSED" : "RELEASED") << std::endl;
    if (pressed) {
        if (button == BTN_LEFT)   { tri_color[0] = 0.2f; tri_color[1] = 0.8f; tri_color[2] = 0.2f; } // Green
        if (button == BTN_RIGHT)  { tri_color[0] = 0.2f; tri_color[1] = 0.4f; tri_color[2] = 1.0f; } // Blue
        if (button == BTN_MIDDLE) { tri_color[0] = 1.0f; tri_color[1] = 1.0f; tri_color[2] = 0.2f; } // Yellow
    }
}
static void pointer_axis(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)pointer; (void)time;
    std::cout << "[Mouse] Scroll axis " << axis << ": " << wl_fixed_to_double(value) << std::endl;
}
static void pointer_frame(void* data, struct wl_pointer* pointer) { (void)data; (void)pointer; }
static void pointer_axis_source(void* data, struct wl_pointer* pointer, uint32_t axis_source) { (void)data; (void)pointer; (void)axis_source; }
static void pointer_axis_stop(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis) { (void)data; (void)pointer; (void)time; (void)axis; }
static void pointer_axis_discrete(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete) { (void)data; (void)pointer; (void)axis; (void)discrete; }
static void pointer_axis_value120(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t value120) { (void)data; (void)pointer; (void)axis; (void)value120; }

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    .axis_value120 = pointer_axis_value120,
    .axis_relative_direction = nullptr,
    .warp = nullptr,
};

// -------------------------------------------------------------
// KEYBOARD INPUT (libxkbcommon)
// -------------------------------------------------------------
static void keyboard_keymap(void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size) {
    (void)data; (void)keyboard;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    char* map_shm = (char*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_shm != MAP_FAILED) {
        if (xkb_map) xkb_keymap_unref(xkb_map);
        xkb_map = xkb_keymap_new_from_string(xkb_ctx, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map_shm, size);
        close(fd);
        if (xkb_map) {
            if (xkb_st) xkb_state_unref(xkb_st);
            xkb_st = xkb_state_new(xkb_map);
        }
    } else {
        close(fd);
    }
}
static void keyboard_enter(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                           struct wl_surface* surface, struct wl_array* keys) {
    (void)data; (void)keyboard; (void)serial; (void)surface; (void)keys;
    std::cout << "[Keyboard] Focus GAINED" << std::endl;
}
static void keyboard_leave(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface) {
    (void)data; (void)keyboard; (void)serial; (void)surface;
    std::cout << "[Keyboard] Focus LOST" << std::endl;
}
static void keyboard_key(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)keyboard; (void)serial; (void)time;
    uint32_t scancode = key + 8; // evdev -> xkb scancode offset
    xkb_keysym_t sym = XKB_KEY_NoSymbol;
    char key_name[64] = "Unknown";
    if (xkb_st) {
        sym = xkb_state_key_get_one_sym(xkb_st, scancode);
        xkb_keysym_get_name(sym, key_name, sizeof(key_name));
    }
    bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    std::cout << "[Keyboard] " << (pressed ? "PRESS:   " : "RELEASE: ")
              << key_name << " (evdev: " << key << ")" << std::endl;
    if (pressed) {
        if (sym == XKB_KEY_Escape) {
            std::cout << "[Keyboard] Escape pressed -> Exiting." << std::endl;
            running = false;
        } else if (sym == XKB_KEY_space) {
            tri_color[0] = 1.0f; tri_color[1] = 0.5f; tri_color[2] = 0.2f; // Reset orange
        } else if (sym == XKB_KEY_r || sym == XKB_KEY_R) {
            tri_color[0] = 1.0f; tri_color[1] = 0.1f; tri_color[2] = 0.1f; // Red
        } else if (sym == XKB_KEY_g || sym == XKB_KEY_G) {
            tri_color[0] = 0.1f; tri_color[1] = 1.0f; tri_color[2] = 0.1f; // Green
        } else if (sym == XKB_KEY_b || sym == XKB_KEY_B) {
            tri_color[0] = 0.1f; tri_color[1] = 0.2f; tri_color[2] = 1.0f; // Blue
        }
    }
}
static void keyboard_modifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                               uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    (void)data; (void)keyboard; (void)serial;
    if (xkb_st) xkb_state_update_mask(xkb_st, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}
static void keyboard_repeat_info(void* data, struct wl_keyboard* keyboard, int32_t rate, int32_t delay) {
    (void)data; (void)keyboard; (void)rate; (void)delay;
}
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

// -------------------------------------------------------------
// TOUCH CONTROLS & GESTURE RECOGNITION (Tap, Long-Press, Pinch)
// -------------------------------------------------------------
struct TouchPoint {
    int32_t  id;
    double   start_x, start_y;
    double   current_x, current_y;
    uint32_t start_time;
    bool     long_pressed = false;
    bool     moved = false;
};

static std::unordered_map<int32_t, TouchPoint> active_touches;
static double initial_pinch_dist = -1.0;
static double last_pinch_dist    = -1.0;

// Periodically check if an unmoving finger has been held >= 500ms
static void check_touch_long_press() {
    uint32_t now = get_time_ms();
    for (auto& [id, pt] : active_touches) {
        if (!pt.long_pressed && !pt.moved && active_touches.size() == 1) {
            if ((now - pt.start_time) >= 500) {
                pt.long_pressed = true;
                std::cout << "\n>>> [Touch Gesture] LONG PRESS DETECTED at ("
                          << (int)pt.current_x << ", " << (int)pt.current_y << ") [held > 500ms] <<<\n" << std::endl;
                // Visual feedback: Gold
                tri_color[0] = 1.0f; tri_color[1] = 0.85f; tri_color[2] = 0.1f;
            }
        }
    }
}

static void touch_down(void* data, struct wl_touch* touch, uint32_t serial, uint32_t time,
                       struct wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)touch; (void)serial; (void)surface;
    double px = wl_fixed_to_double(x);
    double py = wl_fixed_to_double(y);

    TouchPoint pt{
        .id = id,
        .start_x = px,
        .start_y = py,
        .current_x = px,
        .current_y = py,
        .start_time = get_time_ms(),
        .long_pressed = false,
        .moved = false
    };
    active_touches[id] = pt;

    std::cout << "[Touch] Finger DOWN (id: " << id << ") at (" << (int)px << ", " << (int)py << ")"
              << " | Total fingers on screen: " << active_touches.size() << std::endl;

    if (active_touches.size() == 1) {
        tri_color[0] = 1.0f; tri_color[1] = 0.4f; tri_color[2] = 0.7f; // Coral/Pink
    } else if (active_touches.size() == 2) {
        // Initialize two-finger pinch tracking
        auto it = active_touches.begin();
        auto p1 = it->second; ++it;
        auto p2 = it->second;
        double dx = p1.current_x - p2.current_x;
        double dy = p1.current_y - p2.current_y;
        initial_pinch_dist = std::sqrt(dx * dx + dy * dy);
        last_pinch_dist = initial_pinch_dist;
        std::cout << "\n>>> [Touch Gesture] 2-FINGER GESTURE STARTED | Initial Span: "
                  << (int)initial_pinch_dist << "px <<<\n" << std::endl;
        tri_color[0] = 0.2f; tri_color[1] = 0.9f; tri_color[2] = 0.9f; // Turquoise
    } else if (active_touches.size() >= 3) {
        std::cout << "[Touch Gesture] " << active_touches.size() << "-FINGER MULTI-TOUCH CONTACT" << std::endl;
    }
}

static void touch_up(void* data, struct wl_touch* touch, uint32_t serial, uint32_t time, int32_t id) {
    (void)data; (void)touch; (void)serial; (void)time;
    auto it = active_touches.find(id);
    if (it != active_touches.end()) {
        TouchPoint pt = it->second;
        uint32_t duration = get_time_ms() - pt.start_time;

        std::cout << "[Touch] Finger UP (id: " << id << ") [active duration: " << duration << "ms]" << std::endl;

        // If tap criteria satisfied (short hold, minimal motion, not a completed long-press)
        if (!pt.long_pressed && !pt.moved && duration < 350) {
            std::cout << "\n>>> [Touch Gesture] TAP DETECTED at ("
                      << (int)pt.start_x << ", " << (int)pt.start_y << ") <<<\n" << std::endl;
            tri_color[0] = 0.3f; tri_color[1] = 1.0f; tri_color[2] = 0.3f; // Lime Green
        }
        active_touches.erase(it);
    }

    if (active_touches.size() < 2) {
        initial_pinch_dist = -1.0;
        last_pinch_dist    = -1.0;
    }
}

static void touch_motion(void* data, struct wl_touch* touch, uint32_t time,
                         int32_t id, wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)touch; (void)time;
    double px = wl_fixed_to_double(x);
    double py = wl_fixed_to_double(y);

    auto it = active_touches.find(id);
    if (it == active_touches.end()) return;

    it->second.current_x = px;
    it->second.current_y = py;

    // Check if finger moved away from initial contact point
    double move_dist = std::hypot(px - it->second.start_x, py - it->second.start_y);
    if (move_dist > 12.0) {
        it->second.moved = true;
    }

    // 1-Finger Drag / Swipe
    if (active_touches.size() == 1 && it->second.moved) {
        std::cout << "[Touch Gesture] 1-FINGER DRAG to (" << (int)px << ", " << (int)py
                  << ") | delta: (" << (int)(px - it->second.start_x)
                  << ", " << (int)(py - it->second.start_y) << ")" << std::endl;
    }
    // 2-Finger Pinch-to-Zoom & Pan
    else if (active_touches.size() == 2 && initial_pinch_dist > 0.0) {
        auto iter = active_touches.begin();
        auto p1 = iter->second; ++iter;
        auto p2 = iter->second;

        double current_dist = std::hypot(p1.current_x - p2.current_x, p1.current_y - p2.current_y);
        double delta = current_dist - last_pinch_dist;
        double scale = current_dist / initial_pinch_dist;

        if (std::abs(delta) > 2.5) {
            std::cout << "[Touch Gesture] PINCH " << (delta > 0 ? "ZOOM-IN" : "ZOOM-OUT")
                      << " | Scale: " << std::fixed << std::setprecision(2) << scale << "x"
                      << " | Span: " << (int)current_dist << "px "
                      << make_gauge((float)(scale * 50.0f)) << std::endl;
            last_pinch_dist = current_dist;
        }

        // Two-finger midpoint pan
        double mid_x = (p1.current_x + p2.current_x) * 0.5;
        double mid_y = (p1.current_y + p2.current_y) * 0.5;
        std::cout << "[Touch Gesture] 2-FINGER PAN | Center: (" << (int)mid_x << ", " << (int)mid_y << ")" << std::endl;
    }
}

static void touch_frame(void* data, struct wl_touch* touch) { (void)data; (void)touch; }
static void touch_cancel(void* data, struct wl_touch* touch) {
    (void)data; (void)touch;
    std::cout << "[Touch] Multi-touch gesture cancelled by compositor." << std::endl;
    active_touches.clear();
    initial_pinch_dist = -1.0;
}
static void touch_shape(void* data, struct wl_touch* touch, int32_t id, wl_fixed_t major, wl_fixed_t minor) {
    (void)data; (void)touch; (void)id; (void)major; (void)minor;
}
static void touch_orientation(void* data, struct wl_touch* touch, int32_t id, wl_fixed_t orientation) {
    (void)data; (void)touch; (void)id; (void)orientation;
}
static const struct wl_touch_listener touch_listener = {
    .down = touch_down,
    .up = touch_up,
    .motion = touch_motion,
    .frame = touch_frame,
    .cancel = touch_cancel,
    .shape = touch_shape,
    .orientation = touch_orientation,
};

// -------------------------------------------------------------
// GRAPHICS TABLET & STYLUS (tablet-v2 protocol)
// -------------------------------------------------------------
static void tablet_name(void* data, struct zwp_tablet_v2* tab, const char* name) {
    (void)data; (void)tab;
    std::cout << "[Tablet] Hardware Model: " << (name ? name : "Unknown Tablet") << std::endl;
}
static void tablet_id(void* data, struct zwp_tablet_v2* tab, uint32_t vid, uint32_t pid) {
    (void)data; (void)tab;
    std::cout << "[Tablet] USB Vendor:Product ID = " << std::hex << vid << ":" << pid << std::dec << std::endl;
}
static void tablet_path(void* data, struct zwp_tablet_v2* tab, const char* path) {
    (void)data; (void)tab;
    std::cout << "[Tablet] Device Node: " << (path ? path : "none") << std::endl;
}
static void tablet_done(void* data, struct zwp_tablet_v2* tab) { (void)data; (void)tab; }
static void tablet_removed(void* data, struct zwp_tablet_v2* tab) {
    (void)data;
    std::cout << "[Tablet] Tablet disconnected." << std::endl;
    zwp_tablet_v2_destroy(tab);
}
static void tablet_bustype(void* data, struct zwp_tablet_v2* tab, uint32_t bustype) {
    (void)data; (void)tab; (void)bustype;
}
static const struct zwp_tablet_v2_listener tablet_listener = {
    .name = tablet_name,
    .id = tablet_id,
    .path = tablet_path,
    .done = tablet_done,
    .removed = tablet_removed,
    .bustype = tablet_bustype,
};

static void tablet_tool_type(void* data, struct zwp_tablet_tool_v2* tool, uint32_t tool_type) {
    (void)data; (void)tool;
    const char* t = "Generic Stylus";
    if (tool_type == ZWP_TABLET_TOOL_V2_TYPE_PEN) t = "Drawing Pen";
    else if (tool_type == ZWP_TABLET_TOOL_V2_TYPE_ERASER) t = "Eraser Tip";
    else if (tool_type == ZWP_TABLET_TOOL_V2_TYPE_BRUSH) t = "Art Brush";
    else if (tool_type == ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH) t = "Airbrush";
    std::cout << "[Tablet Stylus] Active Tool Type: " << t << std::endl;
}
static void tablet_tool_hardware_serial(void* data, struct zwp_tablet_tool_v2* tool, uint32_t hi, uint32_t lo) {
    (void)data; (void)tool;
    std::cout << "[Tablet Stylus] Hardware Serial: 0x" << std::hex << hi << lo << std::dec << std::endl;
}
static void tablet_tool_hardware_id_wacom(void* data, struct zwp_tablet_tool_v2* tool, uint32_t hi, uint32_t lo) {
    (void)data; (void)tool; (void)hi; (void)lo;
}
static void tablet_tool_capability(void* data, struct zwp_tablet_tool_v2* tool, uint32_t capability) {
    (void)data; (void)tool; (void)capability;
}
static void tablet_tool_done(void* data, struct zwp_tablet_tool_v2* tool) { (void)data; (void)tool; }
static void tablet_tool_removed(void* data, struct zwp_tablet_tool_v2* tool) {
    (void)data;
    std::cout << "[Tablet Stylus] Tool removed from system." << std::endl;
    zwp_tablet_tool_v2_destroy(tool);
}
static void tablet_tool_proximity_in(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial,
                                     struct zwp_tablet_v2* tablet, struct wl_surface* surface) {
    (void)data; (void)tool; (void)serial; (void)tablet; (void)surface;
    std::cout << "\n>>> [Tablet Stylus] PROXIMITY ENTER (Hovering above tablet sensor) <<<\n" << std::endl;
}
static void tablet_tool_proximity_out(void* data, struct zwp_tablet_tool_v2* tool) {
    (void)data; (void)tool;
    std::cout << "\n>>> [Tablet Stylus] PROXIMITY LEAVE (Pen lifted away from sensor) <<<\n" << std::endl;
}
static void tablet_tool_down(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial) {
    (void)data; (void)tool; (void)serial;
    std::cout << "\n>>> [Tablet Stylus] TIP DOWN (Surface Contact / Drawing Stroke Begin) <<<\n" << std::endl;
    tri_color[0] = 0.0f; tri_color[1] = 0.9f; tri_color[2] = 0.9f; // Turquoise
}
static void tablet_tool_up(void* data, struct zwp_tablet_tool_v2* tool) {
    (void)data; (void)tool;
    std::cout << "\n>>> [Tablet Stylus] TIP UP (Stroke Complete) <<<\n" << std::endl;
}
static void tablet_tool_motion(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)tool;
    std::cout << "[Tablet Stylus] Position: (" << std::fixed << std::setprecision(1)
              << wl_fixed_to_double(x) << ", " << wl_fixed_to_double(y) << ")" << std::endl;
}
static void tablet_tool_pressure(void* data, struct zwp_tablet_tool_v2* tool, uint32_t pressure) {
    (void)data; (void)tool;
    float p_ratio = (float)pressure / 65535.0f;
    int p_pct = (int)(p_ratio * 100.0f);
    std::cout << "[Tablet Stylus] Pressure: " << std::setw(3) << p_pct << "% "
              << make_gauge(p_ratio * 100.0f) << " (" << pressure << " / 65535)" << std::endl;
    // Modulate triangle color with pressure
    tri_color[0] = p_ratio;
    tri_color[1] = 0.2f;
    tri_color[2] = 1.0f - p_ratio;
}
static void tablet_tool_distance(void* data, struct zwp_tablet_tool_v2* tool, uint32_t distance) {
    (void)data; (void)tool;
    std::cout << "[Tablet Stylus] Hover Height Distance: " << (distance * 100 / 65535) << "%" << std::endl;
}
static void tablet_tool_tilt(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t tilt_x, wl_fixed_t tilt_y) {
    (void)data; (void)tool;
    std::cout << "[Tablet Stylus] 2D Tilt: (X: " << std::fixed << std::setprecision(1) << wl_fixed_to_double(tilt_x)
              << "°, Y: " << wl_fixed_to_double(tilt_y) << "°)" << std::endl;
}
static void tablet_tool_rotation(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t degrees) {
    (void)data; (void)tool;
    std::cout << "[Tablet Stylus] Z-Rotation: " << wl_fixed_to_double(degrees) << "°" << std::endl;
}
static void tablet_tool_slider(void* data, struct zwp_tablet_tool_v2* tool, int32_t position) {
    (void)data; (void)tool; (void)position;
}
static void tablet_tool_wheel(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t degrees, int32_t clicks) {
    (void)data; (void)tool; (void)degrees; (void)clicks;
}
static void tablet_tool_button(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial, uint32_t button, uint32_t state) {
    (void)data; (void)tool; (void)serial;
    const char* bname = (button == 0x14b) ? "Primary Barrel" : (button == 0x14c) ? "Secondary Barrel" : "Button";
    std::cout << "[Tablet Stylus] " << bname << " (0x" << std::hex << button << std::dec << ") "
              << (state ? "PRESSED (Shortcut/Picker)" : "RELEASED") << std::endl;
    if (state) {
        tri_color[0] = 0.8f; tri_color[1] = 0.2f; tri_color[2] = 0.8f; // Purple
    }
}
static void tablet_tool_frame(void* data, struct zwp_tablet_tool_v2* tool, uint32_t time) {
    (void)data; (void)tool; (void)time;
}
static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
    .type = tablet_tool_type,
    .hardware_serial = tablet_tool_hardware_serial,
    .hardware_id_wacom = tablet_tool_hardware_id_wacom,
    .capability = tablet_tool_capability,
    .done = tablet_tool_done,
    .removed = tablet_tool_removed,
    .proximity_in = tablet_tool_proximity_in,
    .proximity_out = tablet_tool_proximity_out,
    .down = tablet_tool_down,
    .up = tablet_tool_up,
    .motion = tablet_tool_motion,
    .pressure = tablet_tool_pressure,
    .distance = tablet_tool_distance,
    .tilt = tablet_tool_tilt,
    .rotation = tablet_tool_rotation,
    .slider = tablet_tool_slider,
    .wheel = tablet_tool_wheel,
    .button = tablet_tool_button,
    .frame = tablet_tool_frame,
};

static void tablet_seat_tablet_added(void* data, struct zwp_tablet_seat_v2* seat, struct zwp_tablet_v2* tab) {
    (void)data; (void)seat;
    std::cout << "[Tablet Seat] Digitizer Tablet detected." << std::endl;
    zwp_tablet_v2_add_listener(tab, &tablet_listener, nullptr);
}
static void tablet_seat_tool_added(void* data, struct zwp_tablet_seat_v2* seat, struct zwp_tablet_tool_v2* tool) {
    (void)data; (void)seat;
    std::cout << "[Tablet Seat] Stylus / Pen tool paired." << std::endl;
    zwp_tablet_tool_v2_add_listener(tool, &tablet_tool_listener, nullptr);
}
static void tablet_seat_pad_added(void* data, struct zwp_tablet_seat_v2* seat, struct zwp_tablet_pad_v2* pad) {
    (void)data; (void)seat; (void)pad;
    std::cout << "[Tablet Seat] Tablet ExpressKeys / Pad bezel controls available." << std::endl;
}
static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    .tablet_added = tablet_seat_tablet_added,
    .tool_added = tablet_seat_tool_added,
    .pad_added = tablet_seat_pad_added,
};

static void bind_tablet_seat_if_ready() {
    if (tablet_mgr && wl_st && !tablet_seat) {
        tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(tablet_mgr, wl_st);
        zwp_tablet_seat_v2_add_listener(tablet_seat, &tablet_seat_listener, nullptr);
        std::cout << "[Tablet] Tablet seat bound successfully." << std::endl;
    }
}

// -------------------------------------------------------------
// WAYLAND SEAT (Capabilities: Keyboard, Pointer, Touch)
// -------------------------------------------------------------
static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t capabilities) {
    (void)data;
    bool has_pointer  = (capabilities & WL_SEAT_CAPABILITY_POINTER);
    bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD);
    bool has_touch    = (capabilities & WL_SEAT_CAPABILITY_TOUCH);

    if (has_pointer && !wl_ptr) {
        wl_ptr = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(wl_ptr, &pointer_listener, nullptr);
        std::cout << "[Seat] Mouse pointer capability bound" << std::endl;
    } else if (!has_pointer && wl_ptr) {
        wl_pointer_release(wl_ptr);
        wl_ptr = nullptr;
        std::cout << "[Seat] Mouse pointer released" << std::endl;
    }

    if (has_keyboard && !wl_kbd) {
        wl_kbd = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(wl_kbd, &keyboard_listener, nullptr);
        std::cout << "[Seat] Keyboard capability bound" << std::endl;
    } else if (!has_keyboard && wl_kbd) {
        wl_keyboard_release(wl_kbd);
        wl_kbd = nullptr;
        std::cout << "[Seat] Keyboard released" << std::endl;
    }

    if (has_touch && !wl_tch) {
        wl_tch = wl_seat_get_touch(seat);
        wl_touch_add_listener(wl_tch, &touch_listener, nullptr);
        std::cout << "[Seat] Touchscreen capability bound (Multi-touch enabled)" << std::endl;
    } else if (!has_touch && wl_tch) {
        wl_touch_release(wl_tch);
        wl_tch = nullptr;
        std::cout << "[Seat] Touchscreen released" << std::endl;
    }

    bind_tablet_seat_if_ready();
}

static void seat_name(void* data, struct wl_seat* seat, const char* name) {
    (void)data; (void)seat;
    std::cout << "[Seat] Name: " << (name ? name : "default") << std::endl;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

// -------------------------------------------------------------
// WAYLAND REGISTRY
// -------------------------------------------------------------
static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
    (void)data; (void)version;
    if (std::strcmp(interface, "wl_compositor") == 0) {
        wl_comp = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
        xdg_wm = (struct xdg_wm_base*)wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm, &wm_base_listener, nullptr);
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        wl_st = (struct wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 5);
        wl_seat_add_listener(wl_st, &seat_listener, nullptr);
    } else if (std::strcmp(interface, "zwp_tablet_manager_v2") == 0) {
        tablet_mgr = (struct zwp_tablet_manager_v2*)wl_registry_bind(registry, id, &zwp_tablet_manager_v2_interface, 1);
        std::cout << "[Registry] Bound zwp_tablet_manager_v2" << std::endl;
        bind_tablet_seat_if_ready();
    }
}
static void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t id) {
    (void)data; (void)registry; (void)id;
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// -------------------------------------------------------------
// LINUX DIRECT GAME CONTROLLER (/dev/input/js*)
// -------------------------------------------------------------
void init_gamepad() {
    gamepad_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (gamepad_fd >= 0) {
        char name[128] = "Unknown Controller";
        if (ioctl(gamepad_fd, JSIOCGNAME(sizeof(name)), name) >= 0) gamepad_name = name;
        std::cout << "[Gamepad] Connected: " << gamepad_name << " (/dev/input/js0)" << std::endl;
    } else {
        std::cout << "[Gamepad] No controller found on /dev/input/js0 (plug one in anytime)" << std::endl;
    }
}

void poll_gamepad() {
    if (gamepad_fd < 0) {
        static int reconnect_counter = 0;
        if (++reconnect_counter % 180 == 0) {
            gamepad_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
            if (gamepad_fd >= 0) {
                char name[128] = "Unknown Controller";
                if (ioctl(gamepad_fd, JSIOCGNAME(sizeof(name)), name) >= 0) gamepad_name = name;
                std::cout << "[Gamepad] Connected: " << gamepad_name << std::endl;
            }
        }
        return;
    }

    struct js_event e;
    while (read(gamepad_fd, &e, sizeof(e)) > 0) {
        uint8_t type = e.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_BUTTON) {
            std::cout << "[Gamepad] Button " << (int)e.number << ": "
                      << (e.value ? "PRESSED" : "RELEASED") << std::endl;
            if (e.value) {
                tri_color[0] = (float)((e.number * 80 + 30) % 256) / 255.0f;
                tri_color[1] = (float)((e.number * 140 + 70) % 256) / 255.0f;
                tri_color[2] = (float)((e.number * 190 + 120) % 256) / 255.0f;
            }
        } else if (type == JS_EVENT_AXIS) {
            if (e.value > 8000 || e.value < -8000 || e.value == 0) {
                std::cout << "[Gamepad] Axis " << (int)e.number << ": " << e.value << std::endl;
            }
        }
    }

    if (errno == ENODEV) {
        std::cout << "[Gamepad] Disconnected!" << std::endl;
        close(gamepad_fd);
        gamepad_fd = -1;
    }
}

// -------------------------------------------------------------
// CLEANUP
// -------------------------------------------------------------
void cleanup() {
    if (gamepad_fd >= 0) close(gamepad_fd);
    if (xkb_st) xkb_state_unref(xkb_st);
    if (xkb_map) xkb_keymap_unref(xkb_map);
    if (xkb_ctx) xkb_context_unref(xkb_ctx);

    if (tablet_seat) zwp_tablet_seat_v2_destroy(tablet_seat);
    if (tablet_mgr) zwp_tablet_manager_v2_destroy(tablet_mgr);

    if (wl_tch) wl_touch_release(wl_tch);
    if (wl_ptr) wl_pointer_release(wl_ptr);
    if (wl_kbd) wl_keyboard_release(wl_kbd);
    if (wl_st) wl_seat_destroy(wl_st);

    if (xdg_top) xdg_toplevel_destroy(xdg_top);
    if (xdg_surf) xdg_surface_destroy(xdg_surf);
    if (xdg_wm) xdg_wm_base_destroy(xdg_wm);

    if (wl_egl_win) wl_egl_window_destroy(wl_egl_win);
    if (wl_surf) wl_surface_destroy(wl_surf);
    if (wl_comp) wl_compositor_destroy(wl_comp);

    if (egl_disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_ctx != EGL_NO_CONTEXT) eglDestroyContext(egl_disp, egl_ctx);
        if (egl_surface != EGL_NO_SURFACE) eglDestroySurface(egl_disp, egl_surface);
        eglTerminate(egl_disp);
    }

    if (wl_disp) wl_display_disconnect(wl_disp);
    std::cout << "[App] Exited cleanly." << std::endl;
}

// -------------------------------------------------------------
// MAIN ENTRY POINT
// -------------------------------------------------------------
int main() {
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    init_gamepad();

    // 1. Wayland Connection
    wl_disp = wl_display_connect(nullptr);
    if (!wl_disp) {
        std::cerr << "Failed to connect to Wayland display." << std::endl;
        return -1;
    }

    struct wl_registry* registry = wl_display_get_registry(wl_disp);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    wl_display_roundtrip(wl_disp);

    if (!wl_comp || !xdg_wm) {
        std::cerr << "Required Wayland compositors/shells missing." << std::endl;
        return -1;
    }

    // 2. Window Setup
    wl_surf = wl_compositor_create_surface(wl_comp);
    xdg_surf = xdg_wm_base_get_xdg_surface(xdg_wm, wl_surf);
    xdg_surface_add_listener(xdg_surf, &xdg_surface_listener, nullptr);

    xdg_top = xdg_surface_get_toplevel(xdg_surf);
    xdg_toplevel_add_listener(xdg_top, &xdg_toplevel_listener, nullptr);

    xdg_toplevel_set_fullscreen(xdg_top, nullptr);
    wl_surface_commit(wl_surf);
    wl_display_roundtrip(wl_disp);

    // 3. EGL Initialization
    egl_disp = eglGetDisplay((EGLNativeDisplayType)wl_disp);
    eglInitialize(egl_disp, nullptr, nullptr);
    eglBindAPI(EGL_OPENGL_API);

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    EGLConfig egl_cfg; EGLint num_configs;
    eglChooseConfig(egl_disp, attribs, &egl_cfg, 1, &num_configs);
    egl_ctx = eglCreateContext(egl_disp, egl_cfg, EGL_NO_CONTEXT, nullptr);

    wl_egl_win = wl_egl_window_create(wl_surf, 1920, 1080);
    egl_surface = eglCreateWindowSurface(egl_disp, egl_cfg, (EGLNativeWindowType)wl_egl_win, nullptr);
    eglMakeCurrent(egl_disp, egl_surface, egl_surface, egl_ctx);

    std::cout << "SUCCESS: OpenGL context successfully established on window!" << std::endl;

    // 4. OpenGL Shaders & Buffers
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint uColorLoc = glGetUniformLocation(shaderProgram, "uColor");

    float vertices[] = { -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f,  0.5f, 0.0f };
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 5. Main Execution Loop
    while (running) {
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

        // Check for held touch gestures (long-press)
        check_touch_long_press();

        // Poll game controller
        poll_gamepad();

        // OpenGL render passes
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform3f(uColorLoc, tri_color[0], tri_color[1], tri_color[2]);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        eglSwapBuffers(egl_disp, egl_surface);
    }

    cleanup();
    return 0;
}
