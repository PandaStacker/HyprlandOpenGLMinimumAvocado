#define GL_GLEXT_PROTOTYPES
#include "wayland_window.h"

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
#include <unordered_map>
#include <vector>

#include "xdg-shell-client-protocol.h"
#include "tablet-v2-client-protocol.h"

// -----------------------------------------------------------------------------
// Touch Tracking Data
// -----------------------------------------------------------------------------
struct TouchPoint {
    int32_t  id;
    double   start_x, start_y;
    double   current_x, current_y;
    uint32_t start_time;
    bool     long_pressed = false;
    bool     moved        = false;
};

// -----------------------------------------------------------------------------
// Gamepad Metadata
// -----------------------------------------------------------------------------
enum GamepadAxisType { AXIS_THUMBSTICK, AXIS_TRIGGER, AXIS_DPAD };
struct GamepadAxisInfo {
    const char*     name;
    GamepadAxisType type;
};

static inline GamepadAxisInfo get_axis_info(uint8_t axis) {
    switch (axis) {
        case 0:  return { "Left Stick X",  AXIS_THUMBSTICK };
        case 1:  return { "Left Stick Y",  AXIS_THUMBSTICK };
        case 2:  return { "Left Trigger",  AXIS_TRIGGER };
        case 3:  return { "Right Stick X", AXIS_THUMBSTICK };
        case 4:  return { "Right Stick Y", AXIS_THUMBSTICK };
        case 5:  return { "Right Trigger", AXIS_TRIGGER };
        case 6:  return { "D-Pad X",       AXIS_DPAD };
        case 7:  return { "D-Pad Y",       AXIS_DPAD };
        default: return { "Unknown Axis",  AXIS_THUMBSTICK };
    }
}

static inline uint32_t current_time_ms() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// -----------------------------------------------------------------------------
// WaylandWindowImpl Definition
// -----------------------------------------------------------------------------
class WaylandWindowImpl {
public:
    WindowConfig config;
    bool running = true;
    int current_width = 1280;
    int current_height = 720;
    bool fullscreen_state = false;

    // Wayland Handles
    struct wl_display*            wl_disp     = nullptr;
    struct wl_compositor*         wl_comp     = nullptr;
    struct wl_surface*            wl_surf     = nullptr;
    struct wl_egl_window*         wl_egl_win  = nullptr;
    struct xdg_wm_base*           xdg_wm      = nullptr;
    struct xdg_surface*           xdg_surf    = nullptr;
    struct xdg_toplevel*          xdg_top     = nullptr;
    struct wl_seat*               wl_st       = nullptr;
    struct wl_pointer*            wl_ptr      = nullptr;
    struct wl_keyboard*           wl_kbd      = nullptr;
    struct wl_touch*              wl_tch      = nullptr;
    struct zwp_tablet_manager_v2* tablet_mgr  = nullptr;
    struct zwp_tablet_seat_v2*    tablet_seat = nullptr;
    struct zwp_tablet_tool_v2*    tablet_tool = nullptr;

    // XKB Keyboard
    struct xkb_context* xkb_ctx = nullptr;
    struct xkb_keymap*  xkb_map = nullptr;
    struct xkb_state*   xkb_st  = nullptr;

    // EGL Handles
    EGLDisplay egl_disp    = EGL_NO_DISPLAY;
    EGLContext egl_ctx     = EGL_NO_CONTEXT;
    EGLSurface egl_surface = EGL_NO_SURFACE;

    // Direct Linux Gamepad
    int         gamepad_fd   = -1;
    std::string gamepad_name = "None";

    // Touch & Gesture tracking
    std::unordered_map<int32_t, TouchPoint> active_touches;
    double initial_pinch_dist = -1.0;
    double last_pinch_dist    = -1.0;

    // Input Mapper
    InputMapper mapper;

    // Callbacks
    WaylandWindow::ResizeCallback      resize_cb;
    WaylandWindow::KeyCallback         key_cb;
    WaylandWindow::MouseButtonCallback mouse_button_cb;
    WaylandWindow::MouseMotionCallback mouse_motion_cb;
    WaylandWindow::TouchCallback       touch_cb;
    WaylandWindow::StylusCallback      stylus_cb;

    // Listeners
    struct wl_registry_listener           registry_listener{};
    struct xdg_wm_base_listener           wm_base_listener{};
    struct xdg_surface_listener           xdg_surface_listener{};
    struct xdg_toplevel_listener          xdg_toplevel_listener{};
    struct wl_seat_listener               seat_listener{};
    struct wl_pointer_listener            pointer_listener{};
    struct wl_keyboard_listener           keyboard_listener{};
    struct wl_touch_listener              touch_listener{};
    struct zwp_tablet_seat_v2_listener    tablet_seat_listener{};
    struct zwp_tablet_v2_listener         tablet_listener{};
    struct zwp_tablet_tool_v2_listener    tablet_tool_listener{};

    explicit WaylandWindowImpl(const WindowConfig& cfg);
    ~WaylandWindowImpl();

    void init_wayland_and_egl();
    void init_listeners();
    void init_gamepad();
    void poll_gamepad();
    void check_touch_gestures();
};

// -----------------------------------------------------------------------------
// Callback Forwarders (Extract `this` from `void* data`)
// -----------------------------------------------------------------------------
static void cb_xdg_toplevel_configure(void* data, struct xdg_toplevel* top, int32_t width, int32_t height, struct wl_array* states) {
    (void)top; (void)states;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (width > 0 && height > 0 && self->wl_egl_win) {
        self->current_width = width;
        self->current_height = height;
        wl_egl_window_resize(self->wl_egl_win, width, height, 0, 0);
        glViewport(0, 0, width, height);
        if (self->resize_cb) {
            self->resize_cb(width, height);
        }
    }
}

static void cb_xdg_toplevel_close(void* data, struct xdg_toplevel* top) {
    (void)top;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    std::cout << "[Window] Close requested by compositor." << std::endl;
    self->running = false;
}

static void cb_xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(surface, serial);
}

static void cb_wm_base_ping(void* data, struct xdg_wm_base* wm, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm, serial);
}

static void cb_pointer_enter(void* data, struct wl_pointer* ptr, uint32_t serial, struct wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)ptr; (void)serial; (void)surf; (void)sx; (void)sy;
}

static void cb_pointer_leave(void* data, struct wl_pointer* ptr, uint32_t serial, struct wl_surface* surf) {
    (void)data; (void)ptr; (void)serial; (void)surf;
}

static void cb_pointer_motion(void* data, struct wl_pointer* ptr, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    (void)ptr; (void)time;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);
    if (self->mouse_motion_cb) {
        self->mouse_motion_cb(x, y);
    }
}

static void cb_pointer_button(void* data, struct wl_pointer* ptr, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)ptr; (void)serial; (void)time;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    bool pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
    self->mapper.process_digital(InputDevice::Mouse, button, pressed);
    if (self->mouse_button_cb) {
        self->mouse_button_cb(button, pressed, 0.0, 0.0);
    }
}

static void cb_pointer_axis(void* data, struct wl_pointer* ptr, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)ptr; (void)time; (void)axis; (void)value;
}
static void cb_pointer_frame(void* data, struct wl_pointer* ptr) { (void)data; (void)ptr; }
static void cb_pointer_axis_source(void* data, struct wl_pointer* ptr, uint32_t s) { (void)data; (void)ptr; (void)s; }
static void cb_pointer_axis_stop(void* data, struct wl_pointer* ptr, uint32_t t, uint32_t a) { (void)data; (void)ptr; (void)t; (void)a; }
static void cb_pointer_axis_discrete(void* data, struct wl_pointer* ptr, uint32_t a, int32_t d) { (void)data; (void)ptr; (void)a; (void)d; }
static void cb_pointer_axis_value120(void* data, struct wl_pointer* ptr, uint32_t a, int32_t v) { (void)data; (void)ptr; (void)a; (void)v; }

static void cb_keyboard_keymap(void* data, struct wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size) {
    (void)kbd;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    char* map_shm = (char*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_shm != MAP_FAILED) {
        if (self->xkb_map) xkb_keymap_unref(self->xkb_map);
        self->xkb_map = xkb_keymap_new_from_string(self->xkb_ctx, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map_shm, size);
        close(fd);
        if (self->xkb_map) {
            if (self->xkb_st) xkb_state_unref(self->xkb_st);
            self->xkb_st = xkb_state_new(self->xkb_map);
        }
    } else {
        close(fd);
    }
}

static void cb_keyboard_enter(void* data, struct wl_keyboard* kbd, uint32_t serial, struct wl_surface* surf, struct wl_array* keys) {
    (void)data; (void)kbd; (void)serial; (void)surf; (void)keys;
}

static void cb_keyboard_leave(void* data, struct wl_keyboard* kbd, uint32_t serial, struct wl_surface* surf) {
    (void)data; (void)kbd; (void)serial; (void)surf;
}

static void cb_keyboard_key(void* data, struct wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    (void)kbd; (void)serial; (void)time;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    uint32_t scancode = key + 8;
    xkb_keysym_t sym = XKB_KEY_NoSymbol;
    if (self->xkb_st) {
        sym = xkb_state_key_get_one_sym(self->xkb_st, scancode);
    }
    bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    self->mapper.process_digital(InputDevice::Keyboard, sym, pressed);

    if (pressed) {
        if (self->mapper.is_action_down(Action::Quit)) {
            std::cout << "[Action: Quit] Exiting application (exit key triggered)." << std::endl;
            self->running = false;
        } else if (sym == XKB_KEY_underscore && !self->mapper.is_action_enabled(Action::Quit)) {
            std::cout << "[Keyboard] '_' pressed, but exit key is disabled by default for library integration.\n"
                      << "           Enable it via '--enable-exit' argument or by calling window.input().enable_exit_action(true)." << std::endl;
        }
    }

    if (self->key_cb) {
        self->key_cb(key, sym, pressed);
    }
}

static void cb_keyboard_modifiers(void* data, struct wl_keyboard* kbd, uint32_t serial, uint32_t md, uint32_t ml, uint32_t mk, uint32_t g) {
    (void)kbd; (void)serial;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (self->xkb_st) xkb_state_update_mask(self->xkb_st, md, ml, mk, 0, 0, g);
}
static void cb_keyboard_repeat_info(void* data, struct wl_keyboard* kbd, int32_t r, int32_t d) { (void)data; (void)kbd; (void)r; (void)d; }

static void cb_touch_down(void* data, struct wl_touch* tch, uint32_t serial, uint32_t time, struct wl_surface* surf, int32_t id, wl_fixed_t x, wl_fixed_t y) {
    (void)tch; (void)serial; (void)time; (void)surf;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    double px = wl_fixed_to_double(x);
    double py = wl_fixed_to_double(y);

    self->active_touches[id] = TouchPoint{
        .id = id, .start_x = px, .start_y = py,
        .current_x = px, .current_y = py,
        .start_time = current_time_ms(),
        .long_pressed = false, .moved = false
    };

    if (self->active_touches.size() == 2) {
        auto it = self->active_touches.begin();
        auto p1 = it->second; ++it;
        auto p2 = it->second;
        self->initial_pinch_dist = std::hypot(p1.current_x - p2.current_x, p1.current_y - p2.current_y);
        self->last_pinch_dist = self->initial_pinch_dist;
    }

    if (self->touch_cb) self->touch_cb(id, px, py, true);
}

static void cb_touch_up(void* data, struct wl_touch* tch, uint32_t serial, uint32_t time, int32_t id) {
    (void)tch; (void)serial; (void)time;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (self->touch_cb) self->touch_cb(id, 0.0, 0.0, false);
    self->active_touches.erase(id);
    if (self->active_touches.size() < 2) {
        self->initial_pinch_dist = -1.0;
    }
}

static void cb_touch_motion(void* data, struct wl_touch* tch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
    (void)tch; (void)time;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    auto it = self->active_touches.find(id);
    if (it == self->active_touches.end()) return;

    double px = wl_fixed_to_double(x);
    double py = wl_fixed_to_double(y);
    it->second.current_x = px;
    it->second.current_y = py;

    if (std::hypot(px - it->second.start_x, py - it->second.start_y) > 12.0) {
        it->second.moved = true;
    }
    if (self->touch_cb) self->touch_cb(id, px, py, true);
}
static void cb_touch_frame(void* data, struct wl_touch* tch) { (void)data; (void)tch; }
static void cb_touch_cancel(void* data, struct wl_touch* tch) {
    (void)tch;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    self->active_touches.clear();
    self->initial_pinch_dist = -1.0;
}
static void cb_touch_shape(void* d, struct wl_touch* t, int32_t i, wl_fixed_t maj, wl_fixed_t min) { (void)d; (void)t; (void)i; (void)maj; (void)min; }
static void cb_touch_orientation(void* d, struct wl_touch* t, int32_t i, wl_fixed_t o) { (void)d; (void)t; (void)i; (void)o; }

// Stylus Callbacks
static void cb_tablet_tool_proximity_in(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial, struct zwp_tablet_v2* tab, struct wl_surface* surf) {
    (void)data; (void)tool; (void)serial; (void)tab; (void)surf;
}
static void cb_tablet_tool_proximity_out(void* data, struct zwp_tablet_tool_v2* tool) { (void)data; (void)tool; }
static void cb_tablet_tool_down(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial) {
    (void)tool; (void)serial;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    self->mapper.process_digital(InputDevice::Stylus, 1, true);
}
static void cb_tablet_tool_up(void* data, struct zwp_tablet_tool_v2* tool) {
    (void)tool;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    self->mapper.process_digital(InputDevice::Stylus, 1, false);
}
static void cb_tablet_tool_motion(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t x, wl_fixed_t y) {
    (void)tool;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (self->stylus_cb) {
        self->stylus_cb(wl_fixed_to_double(x), wl_fixed_to_double(y), 0.0, 0.0, 0.0);
    }
}
static void cb_tablet_tool_pressure(void* data, struct zwp_tablet_tool_v2* tool, uint32_t pressure) {
    (void)tool;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    float norm = (float)pressure / 65535.0f;
    self->mapper.process_axis(InputDevice::Stylus, 0, norm);
}
static void cb_tablet_tool_distance(void* d, struct zwp_tablet_tool_v2* t, uint32_t dist) { (void)d; (void)t; (void)dist; }
static void cb_tablet_tool_tilt(void* data, struct zwp_tablet_tool_v2* tool, wl_fixed_t tilt_x, wl_fixed_t tilt_y) {
    (void)tool;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (self->stylus_cb) {
        self->stylus_cb(0.0, 0.0, 0.0, wl_fixed_to_double(tilt_x), wl_fixed_to_double(tilt_y));
    }
}
static void cb_tablet_tool_rotation(void* d, struct zwp_tablet_tool_v2* t, wl_fixed_t degrees) { (void)d; (void)t; (void)degrees; }
static void cb_tablet_tool_slider(void* d, struct zwp_tablet_tool_v2* t, int32_t pos) { (void)d; (void)t; (void)pos; }
static void cb_tablet_tool_wheel(void* d, struct zwp_tablet_tool_v2* t, wl_fixed_t deg, int32_t clicks) { (void)d; (void)t; (void)deg; (void)clicks; }
static void cb_tablet_tool_button(void* data, struct zwp_tablet_tool_v2* tool, uint32_t serial, uint32_t button, uint32_t state) {
    (void)tool; (void)serial;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    self->mapper.process_digital(InputDevice::Stylus, button, state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED);
}
static void cb_tablet_tool_frame(void* d, struct zwp_tablet_tool_v2* t, uint32_t time) { (void)d; (void)t; (void)time; }
static void cb_tablet_tool_type(void* d, struct zwp_tablet_tool_v2* t, uint32_t type) { (void)d; (void)t; (void)type; }
static void cb_tablet_tool_hardware_serial(void* d, struct zwp_tablet_tool_v2* t, uint32_t hi, uint32_t lo) { (void)d; (void)t; (void)hi; (void)lo; }
static void cb_tablet_tool_hardware_id_wacom(void* d, struct zwp_tablet_tool_v2* t, uint32_t hi, uint32_t lo) { (void)d; (void)t; (void)hi; (void)lo; }
static void cb_tablet_tool_capability(void* d, struct zwp_tablet_tool_v2* t, uint32_t cap) { (void)d; (void)t; (void)cap; }
static void cb_tablet_tool_done(void* d, struct zwp_tablet_tool_v2* t) { (void)d; (void)t; }
static void cb_tablet_tool_removed(void* data, struct zwp_tablet_tool_v2* tool) {
    (void)tool;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (self->tablet_tool) {
        zwp_tablet_tool_v2_destroy(self->tablet_tool);
        self->tablet_tool = nullptr;
    }
}

static void cb_tablet_seat_tablet_added(void* data, struct zwp_tablet_seat_v2* ts, struct zwp_tablet_v2* tab) {
    (void)ts;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    zwp_tablet_v2_add_listener(tab, &self->tablet_listener, self);
}

static void cb_tablet_seat_tool_added(void* data, struct zwp_tablet_seat_v2* ts, struct zwp_tablet_tool_v2* tool) {
    (void)ts;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    self->tablet_tool = tool;
    zwp_tablet_tool_v2_add_listener(tool, &self->tablet_tool_listener, self);
}

static void cb_tablet_seat_pad_added(void* d, struct zwp_tablet_seat_v2* s, struct zwp_tablet_pad_v2* p) { (void)d; (void)s; (void)p; }

static void cb_seat_capabilities(void* data, struct wl_seat* seat, uint32_t caps) {
    auto* self = static_cast<WaylandWindowImpl*>(data);

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !self->wl_ptr) {
        self->wl_ptr = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(self->wl_ptr, &self->pointer_listener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && self->wl_ptr) {
        wl_pointer_destroy(self->wl_ptr);
        self->wl_ptr = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->wl_kbd) {
        self->wl_kbd = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self->wl_kbd, &self->keyboard_listener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && self->wl_kbd) {
        wl_keyboard_destroy(self->wl_kbd);
        self->wl_kbd = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !self->wl_tch) {
        self->wl_tch = wl_seat_get_touch(seat);
        wl_touch_add_listener(self->wl_tch, &self->touch_listener, self);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && self->wl_tch) {
        wl_touch_destroy(self->wl_tch);
        self->wl_tch = nullptr;
    }
}
static void cb_seat_name(void* data, struct wl_seat* seat, const char* name) { (void)data; (void)seat; (void)name; }

static void cb_registry_handle_global(void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
    (void)version;
    auto* self = static_cast<WaylandWindowImpl*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        self->wl_comp = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        self->xdg_wm = (struct xdg_wm_base*)wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(self->xdg_wm, &self->wm_base_listener, self);
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self->wl_st = (struct wl_seat*)wl_registry_bind(registry, id, &wl_seat_interface, 5);
        wl_seat_add_listener(self->wl_st, &self->seat_listener, self);
    } else if (std::strcmp(interface, zwp_tablet_manager_v2_interface.name) == 0) {
        self->tablet_mgr = (struct zwp_tablet_manager_v2*)wl_registry_bind(registry, id, &zwp_tablet_manager_v2_interface, 1);
    }
}

static void cb_registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t id) {
    (void)data; (void)registry; (void)id;
}

// -----------------------------------------------------------------------------
// WaylandWindowImpl Implementation
// -----------------------------------------------------------------------------
WaylandWindowImpl::WaylandWindowImpl(const WindowConfig& cfg)
    : config(cfg), current_width(cfg.width), current_height(cfg.height), fullscreen_state(cfg.fullscreen) {
    if (cfg.enable_exit_button) {
        mapper.enable_exit_action(true);
    }
    init_listeners();
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    init_gamepad();
    init_wayland_and_egl();
}

WaylandWindowImpl::~WaylandWindowImpl() {
    if (gamepad_fd >= 0) close(gamepad_fd);
    if (xkb_st)  xkb_state_unref(xkb_st);
    if (xkb_map) xkb_keymap_unref(xkb_map);
    if (xkb_ctx) xkb_context_unref(xkb_ctx);

    if (tablet_tool) zwp_tablet_tool_v2_destroy(tablet_tool);
    if (tablet_seat) zwp_tablet_seat_v2_destroy(tablet_seat);
    if (tablet_mgr)  zwp_tablet_manager_v2_destroy(tablet_mgr);

    if (wl_ptr) wl_pointer_destroy(wl_ptr);
    if (wl_kbd) wl_keyboard_destroy(wl_kbd);
    if (wl_tch) wl_touch_destroy(wl_tch);
    if (wl_st)  wl_seat_destroy(wl_st);

    if (egl_disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_surface != EGL_NO_SURFACE) eglDestroySurface(egl_disp, egl_surface);
        if (egl_ctx != EGL_NO_CONTEXT)     eglDestroyContext(egl_disp, egl_ctx);
        eglTerminate(egl_disp);
    }

    if (wl_egl_win) wl_egl_window_destroy(wl_egl_win);
    if (xdg_top)    xdg_toplevel_destroy(xdg_top);
    if (xdg_surf)   xdg_surface_destroy(xdg_surf);
    if (xdg_wm)     xdg_wm_base_destroy(xdg_wm);
    if (wl_surf)    wl_surface_destroy(wl_surf);
    if (wl_comp)    wl_compositor_destroy(wl_comp);

    if (wl_disp) {
        wl_display_disconnect(wl_disp);
    }
}

void WaylandWindowImpl::init_listeners() {
    xdg_toplevel_listener.configure        = cb_xdg_toplevel_configure;
    xdg_toplevel_listener.close            = cb_xdg_toplevel_close;
    xdg_surface_listener.configure         = cb_xdg_surface_configure;
    wm_base_listener.ping                  = cb_wm_base_ping;

    pointer_listener.enter                 = cb_pointer_enter;
    pointer_listener.leave                 = cb_pointer_leave;
    pointer_listener.motion                = cb_pointer_motion;
    pointer_listener.button                = cb_pointer_button;
    pointer_listener.axis                  = cb_pointer_axis;
    pointer_listener.frame                 = cb_pointer_frame;
    pointer_listener.axis_source           = cb_pointer_axis_source;
    pointer_listener.axis_stop             = cb_pointer_axis_stop;
    pointer_listener.axis_discrete         = cb_pointer_axis_discrete;
    pointer_listener.axis_value120         = cb_pointer_axis_value120;

    keyboard_listener.keymap               = cb_keyboard_keymap;
    keyboard_listener.enter                = cb_keyboard_enter;
    keyboard_listener.leave                = cb_keyboard_leave;
    keyboard_listener.key                  = cb_keyboard_key;
    keyboard_listener.modifiers            = cb_keyboard_modifiers;
    keyboard_listener.repeat_info          = cb_keyboard_repeat_info;

    touch_listener.down                    = cb_touch_down;
    touch_listener.up                      = cb_touch_up;
    touch_listener.motion                  = cb_touch_motion;
    touch_listener.frame                   = cb_touch_frame;
    touch_listener.cancel                  = cb_touch_cancel;
    touch_listener.shape                   = cb_touch_shape;
    touch_listener.orientation             = cb_touch_orientation;

    tablet_seat_listener.tablet_added      = cb_tablet_seat_tablet_added;
    tablet_seat_listener.tool_added        = cb_tablet_seat_tool_added;
    tablet_seat_listener.pad_added         = cb_tablet_seat_pad_added;

    tablet_tool_listener.type              = cb_tablet_tool_type;
    tablet_tool_listener.hardware_serial   = cb_tablet_tool_hardware_serial;
    tablet_tool_listener.hardware_id_wacom = cb_tablet_tool_hardware_id_wacom;
    tablet_tool_listener.capability        = cb_tablet_tool_capability;
    tablet_tool_listener.done              = cb_tablet_tool_done;
    tablet_tool_listener.removed           = cb_tablet_tool_removed;
    tablet_tool_listener.proximity_in      = cb_tablet_tool_proximity_in;
    tablet_tool_listener.proximity_out     = cb_tablet_tool_proximity_out;
    tablet_tool_listener.down              = cb_tablet_tool_down;
    tablet_tool_listener.up                = cb_tablet_tool_up;
    tablet_tool_listener.motion            = cb_tablet_tool_motion;
    tablet_tool_listener.pressure          = cb_tablet_tool_pressure;
    tablet_tool_listener.distance          = cb_tablet_tool_distance;
    tablet_tool_listener.tilt              = cb_tablet_tool_tilt;
    tablet_tool_listener.rotation          = cb_tablet_tool_rotation;
    tablet_tool_listener.slider            = cb_tablet_tool_slider;
    tablet_tool_listener.wheel             = cb_tablet_tool_wheel;
    tablet_tool_listener.button            = cb_tablet_tool_button;
    tablet_tool_listener.frame             = cb_tablet_tool_frame;

    seat_listener.capabilities             = cb_seat_capabilities;
    seat_listener.name                     = cb_seat_name;

    registry_listener.global               = cb_registry_handle_global;
    registry_listener.global_remove        = cb_registry_handle_global_remove;
}

void WaylandWindowImpl::init_gamepad() {
    gamepad_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (gamepad_fd >= 0) {
        char name[128] = "Unknown Controller";
        if (ioctl(gamepad_fd, JSIOCGNAME(sizeof(name)), name) >= 0) {
            gamepad_name = name;
        }
        std::cout << "[Gamepad] Connected: " << gamepad_name << std::endl;
    }
}

void WaylandWindowImpl::poll_gamepad() {
    if (gamepad_fd < 0) {
        static uint32_t last_retry = 0;
        uint32_t now = current_time_ms();
        if (now - last_retry > 2000) {
            last_retry = now;
            init_gamepad();
        }
        return;
    }

    struct js_event e;
    while (read(gamepad_fd, &e, sizeof(e)) > 0) {
        uint8_t type = e.type & ~JS_EVENT_INIT;
        if (type == JS_EVENT_BUTTON) {
            mapper.process_digital(InputDevice::Gamepad, e.number, e.value != 0);
        } else if (type == JS_EVENT_AXIS) {
            float norm = (float)e.value / 32767.0f;
            mapper.process_axis(InputDevice::Gamepad, e.number, norm);
        }
    }

    if (errno == ENODEV) {
        std::cout << "[Gamepad] Disconnected!" << std::endl;
        close(gamepad_fd);
        gamepad_fd = -1;
    }
}

void WaylandWindowImpl::check_touch_gestures() {
    uint32_t now = current_time_ms();
    for (auto& [id, pt] : active_touches) {
        if (!pt.long_pressed && !pt.moved && active_touches.size() == 1) {
            if ((now - pt.start_time) >= 500) {
                pt.long_pressed = true;
                std::cout << "[Touch Gesture] Long Press at (" << (int)pt.current_x << ", " << (int)pt.current_y << ")" << std::endl;
            }
        }
    }
}

void WaylandWindowImpl::init_wayland_and_egl() {
    wl_disp = wl_display_connect(nullptr);
    if (!wl_disp) {
        throw std::runtime_error("Failed to connect to Wayland display.");
    }

    struct wl_registry* registry = wl_display_get_registry(wl_disp);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(wl_disp);

    if (!wl_comp || !xdg_wm) {
        throw std::runtime_error("Required Wayland compositor or XDG Shell extension not found.");
    }

    if (tablet_mgr && wl_st) {
        tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(tablet_mgr, wl_st);
        zwp_tablet_seat_v2_add_listener(tablet_seat, &tablet_seat_listener, this);
    }

    // 2. Window Setup
    wl_surf  = wl_compositor_create_surface(wl_comp);
    xdg_surf = xdg_wm_base_get_xdg_surface(xdg_wm, wl_surf);
    xdg_surface_add_listener(xdg_surf, &xdg_surface_listener, this);

    xdg_top = xdg_surface_get_toplevel(xdg_surf);
    xdg_toplevel_add_listener(xdg_top, &xdg_toplevel_listener, this);
    xdg_toplevel_set_title(xdg_top, config.title.c_str());
    xdg_toplevel_set_app_id(xdg_top, config.app_id.c_str());

    if (config.fullscreen) {
        xdg_toplevel_set_fullscreen(xdg_top, nullptr);
    }

    wl_surface_commit(wl_surf);
    wl_display_roundtrip(wl_disp);

    // 3. EGL Initialization
    egl_disp = eglGetDisplay((EGLNativeDisplayType)wl_disp);
    if (egl_disp == EGL_NO_DISPLAY) {
        throw std::runtime_error("Failed to get EGLDisplay from Wayland display.");
    }
    eglInitialize(egl_disp, nullptr, nullptr);
    eglBindAPI(EGL_OPENGL_API);

    std::vector<EGLint> attribs = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        config.red_bits,
        EGL_GREEN_SIZE,      config.green_bits,
        EGL_BLUE_SIZE,       config.blue_bits,
        EGL_ALPHA_SIZE,      config.alpha_bits,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    };
    if (config.depth_bits > 0) {
        attribs.push_back(EGL_DEPTH_SIZE);
        attribs.push_back(config.depth_bits);
    }
    if (config.stencil_bits > 0) {
        attribs.push_back(EGL_STENCIL_SIZE);
        attribs.push_back(config.stencil_bits);
    }
    if (config.samples > 1) {
        attribs.push_back(EGL_SAMPLE_BUFFERS);
        attribs.push_back(1);
        attribs.push_back(EGL_SAMPLES);
        attribs.push_back(config.samples);
    }
    attribs.push_back(EGL_NONE);

    EGLConfig egl_cfg;
    EGLint num_configs = 0;
    eglChooseConfig(egl_disp, attribs.data(), &egl_cfg, 1, &num_configs);

    // If MSAA was requested but not supported on this driver, fallback gracefully
    if (num_configs == 0 && config.samples > 1) {
        std::cerr << "[WaylandWindow] Warning: MSAA " << config.samples << "x unsupported by driver; falling back to 1x." << std::endl;
        std::vector<EGLint> fallback_attribs = {
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_RED_SIZE,        config.red_bits,
            EGL_GREEN_SIZE,      config.green_bits,
            EGL_BLUE_SIZE,       config.blue_bits,
            EGL_ALPHA_SIZE,      config.alpha_bits,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        };
        if (config.depth_bits > 0) {
            fallback_attribs.push_back(EGL_DEPTH_SIZE);
            fallback_attribs.push_back(config.depth_bits);
        }
        if (config.stencil_bits > 0) {
            fallback_attribs.push_back(EGL_STENCIL_SIZE);
            fallback_attribs.push_back(config.stencil_bits);
        }
        fallback_attribs.push_back(EGL_NONE);
        eglChooseConfig(egl_disp, fallback_attribs.data(), &egl_cfg, 1, &num_configs);
    }

    if (num_configs == 0) {
        throw std::runtime_error("Failed to choose suitable EGLConfig for OpenGL.");
    }

    // Context Attributes (Major/Minor version and Core profile)
    std::vector<EGLint> ctx_attribs;
    if (config.gl_major > 0) {
        ctx_attribs.push_back(EGL_CONTEXT_MAJOR_VERSION);
        ctx_attribs.push_back(config.gl_major);
        ctx_attribs.push_back(EGL_CONTEXT_MINOR_VERSION);
        ctx_attribs.push_back(config.gl_minor);
        ctx_attribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK);
        ctx_attribs.push_back(config.gl_core_profile ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT);
    }
    ctx_attribs.push_back(EGL_NONE);

    egl_ctx = eglCreateContext(egl_disp, egl_cfg, EGL_NO_CONTEXT, ctx_attribs.data());
    if (egl_ctx == EGL_NO_CONTEXT) {
        // Fallback to unconstrained default context if requested profile version is unsupported
        egl_ctx = eglCreateContext(egl_disp, egl_cfg, EGL_NO_CONTEXT, nullptr);
        if (egl_ctx == EGL_NO_CONTEXT) {
            throw std::runtime_error("Failed to create EGLContext for OpenGL.");
        }
    }

    wl_egl_win = wl_egl_window_create(wl_surf, config.width, config.height);
    egl_surface = eglCreateWindowSurface(egl_disp, egl_cfg, (EGLNativeWindowType)wl_egl_win, nullptr);
    if (egl_surface == EGL_NO_SURFACE) {
        throw std::runtime_error("Failed to create EGL window surface.");
    }
    eglMakeCurrent(egl_disp, egl_surface, egl_surface, egl_ctx);

    // Apply VSync / swap interval
    eglSwapInterval(egl_disp, config.vsync ? 1 : 0);

    std::cout << "[WaylandWindow] Hardware accelerated context created ("
              << config.width << "x" << config.height
              << " | Depth: " << config.depth_bits << "-bit"
              << " | Stencil: " << config.stencil_bits << "-bit"
              << " | Samples: " << (config.samples > 1 ? config.samples : 1) << "x"
              << " | VSync: " << (config.vsync ? "ON" : "OFF") << ")" << std::endl;
}

// -----------------------------------------------------------------------------
// WaylandWindow Public Facade
// -----------------------------------------------------------------------------
WaylandWindow::WaylandWindow(const WindowConfig& config)
    : impl(std::make_unique<WaylandWindowImpl>(config)) {}

WaylandWindow::WaylandWindow(int width, int height, const std::string& title, bool fullscreen)
    : WaylandWindow(WindowConfig{ .width = width, .height = height, .title = title, .fullscreen = fullscreen }) {}

WaylandWindow::~WaylandWindow() = default;

WaylandWindow::WaylandWindow(WaylandWindow&&) noexcept = default;
WaylandWindow& WaylandWindow::operator=(WaylandWindow&&) noexcept = default;

bool WaylandWindow::is_running() const noexcept {
    return impl && impl->running;
}

void WaylandWindow::close() noexcept {
    if (impl) impl->running = false;
}

void WaylandWindow::poll_events() {
    if (!impl || !impl->wl_disp) return;

    wl_display_flush(impl->wl_disp);

    while (wl_display_prepare_read(impl->wl_disp) != 0) {
        wl_display_dispatch_pending(impl->wl_disp);
    }

    struct pollfd pfd = { wl_display_get_fd(impl->wl_disp), POLLIN, 0 };
    if (poll(&pfd, 1, 0) > 0) {
        wl_display_read_events(impl->wl_disp);
    } else {
        wl_display_cancel_read(impl->wl_disp);
    }
    wl_display_dispatch_pending(impl->wl_disp);

    impl->check_touch_gestures();
    impl->poll_gamepad();
    impl->mapper.post_frame_update();
}

void WaylandWindow::swap_buffers() {
    if (impl && impl->egl_disp != EGL_NO_DISPLAY && impl->egl_surface != EGL_NO_SURFACE) {
        eglSwapBuffers(impl->egl_disp, impl->egl_surface);
    }
}

void WaylandWindow::set_vsync(bool enabled) {
    if (impl && impl->egl_disp != EGL_NO_DISPLAY) {
        impl->config.vsync = enabled;
        eglSwapInterval(impl->egl_disp, enabled ? 1 : 0);
    }
}

const WindowConfig& WaylandWindow::get_config() const noexcept {
    static const WindowConfig default_cfg{};
    return impl ? impl->config : default_cfg;
}

InputMapper& WaylandWindow::input() noexcept {
    return impl->mapper;
}

const InputMapper& WaylandWindow::input() const noexcept {
    return impl->mapper;
}

int WaylandWindow::get_width() const noexcept {
    return impl ? impl->current_width : 0;
}

int WaylandWindow::get_height() const noexcept {
    return impl ? impl->current_height : 0;
}

bool WaylandWindow::is_fullscreen() const noexcept {
    return impl ? impl->fullscreen_state : false;
}

void WaylandWindow::set_fullscreen(bool fullscreen) {
    if (!impl || !impl->xdg_top) return;
    impl->fullscreen_state = fullscreen;
    if (fullscreen) {
        xdg_toplevel_set_fullscreen(impl->xdg_top, nullptr);
    } else {
        xdg_toplevel_unset_fullscreen(impl->xdg_top);
    }
    wl_surface_commit(impl->wl_surf);
}

void WaylandWindow::set_title(const std::string& title) {
    if (!impl || !impl->xdg_top) return;
    impl->config.title = title;
    xdg_toplevel_set_title(impl->xdg_top, title.c_str());
}

void WaylandWindow::on_resize(ResizeCallback cb) {
    if (impl) impl->resize_cb = std::move(cb);
}

void WaylandWindow::on_key(KeyCallback cb) {
    if (impl) impl->key_cb = std::move(cb);
}

void WaylandWindow::on_mouse_button(MouseButtonCallback cb) {
    if (impl) impl->mouse_button_cb = std::move(cb);
}

void WaylandWindow::on_mouse_motion(MouseMotionCallback cb) {
    if (impl) impl->mouse_motion_cb = std::move(cb);
}

void WaylandWindow::on_touch(TouchCallback cb) {
    if (impl) impl->touch_cb = std::move(cb);
}

void WaylandWindow::on_stylus(StylusCallback cb) {
    if (impl) impl->stylus_cb = std::move(cb);
}

void* WaylandWindow::get_wl_display() const noexcept {
    return impl ? (void*)impl->wl_disp : nullptr;
}

void* WaylandWindow::get_wl_surface() const noexcept {
    return impl ? (void*)impl->wl_surf : nullptr;
}

void* WaylandWindow::get_egl_display() const noexcept {
    return impl ? (void*)impl->egl_disp : nullptr;
}

void* WaylandWindow::get_egl_context() const noexcept {
    return impl ? (void*)impl->egl_ctx : nullptr;
}
