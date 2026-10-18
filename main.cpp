// Build with: ./build.sh
// Or: cmake -B build && cmake --build build
// Or: g++ -std=c++26 main.cpp -x c xdg-shell-protocol.c -lwayland-client -lwayland-egl -lEGL -lGL -lxkbcommon -o full_gl
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
#include <cstring>
#include <iostream>
#include <string>

// Additional XDG Shell protocol header
#include "xdg-shell-client-protocol.h"

// Low-level raw pointers for window lifecycle
struct wl_display*     wl_disp      = nullptr;
struct wl_compositor*  wl_comp      = nullptr;
struct wl_surface*     wl_surf      = nullptr;
struct wl_egl_window*  wl_egl_win   = nullptr;
struct xdg_wm_base*    xdg_wm       = nullptr;
struct xdg_surface*    xdg_surf     = nullptr;
struct xdg_toplevel*   xdg_top      = nullptr;

// Wayland Seat & Input devices
struct wl_seat*        wl_st        = nullptr;
struct wl_pointer*     wl_ptr       = nullptr;
struct wl_keyboard*    wl_kbd       = nullptr;

// XKB keyboard mapping state
struct xkb_context*    xkb_ctx      = nullptr;
struct xkb_keymap*     xkb_map      = nullptr;
struct xkb_state*      xkb_st       = nullptr;

// Linux Game Controller
int                    gamepad_fd   = -1;
std::string            gamepad_name = "None";

// EGL context
EGLDisplay             egl_disp     = EGL_NO_DISPLAY;
EGLContext             egl_ctx      = EGL_NO_CONTEXT;
EGLSurface             egl_surface  = EGL_NO_SURFACE;

// Application lifecycle & dynamic render state
bool                   running      = true;
float                  tri_color[3] = { 1.0f, 0.5f, 0.2f }; // Default orange

// Basic Shader Source Code (Modern Programmable OpenGL)
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); }\n";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uColor;\n"
    "void main() { FragColor = vec4(uColor, 1.0f); }\n";

// -------------------------------------------------------------
// XDG Shell Event Handlers
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
    xdg_surface_ack_configure(surface, serial); // Handshake complete
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void wm_base_ping(void* data, struct xdg_wm_base* wm, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm, serial); // Tell Hyprland we haven't crashed
}
static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = wm_base_ping,
};

// -------------------------------------------------------------
// WAYLAND MOUSE / POINTER INPUT
// -------------------------------------------------------------
static void pointer_enter(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    std::cout << "[Mouse] Enter at (" << wl_fixed_to_double(sx) << ", " << wl_fixed_to_double(sy) << ")" << std::endl;
}

static void pointer_leave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
    (void)data; (void)pointer; (void)serial; (void)surface;
    std::cout << "[Mouse] Leave" << std::endl;
}

static void pointer_motion(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data; (void)pointer; (void)time;
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);
    std::cout << "[Mouse] Move: (" << x << ", " << y << ")" << std::endl;
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
        // Change triangle color on mouse click to give instant visual feedback
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
// WAYLAND KEYBOARD INPUT (libxkbcommon)
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
    std::cout << "[Keyboard] Window gained keyboard focus" << std::endl;
}

static void keyboard_leave(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface) {
    (void)data; (void)keyboard; (void)serial; (void)surface;
    std::cout << "[Keyboard] Window lost keyboard focus" << std::endl;
}

static void keyboard_key(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state) {
    (void)data; (void)keyboard; (void)serial; (void)time;
    // Linux evdev scancode -> XKB scancode offset is +8
    uint32_t scancode = key + 8;
    xkb_keysym_t sym = XKB_KEY_NoSymbol;
    char key_name[64] = "Unknown";

    if (xkb_st) {
        sym = xkb_state_key_get_one_sym(xkb_st, scancode);
        xkb_keysym_get_name(sym, key_name, sizeof(key_name));
    }

    bool pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    std::cout << "[Keyboard] " << (pressed ? "PRESS:   " : "RELEASE: ")
              << key_name << " (evdev code: " << key << ")" << std::endl;

    if (pressed) {
        // Escape closes the app cleanly
        if (sym == XKB_KEY_Escape) {
            std::cout << "[Keyboard] Escape pressed -> Exiting cleanly." << std::endl;
            running = false;
        }
        // Change colors with keys
        if (sym == XKB_KEY_space) {
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
    if (xkb_st) {
        xkb_state_update_mask(xkb_st, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    }
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
// WAYLAND SEAT (Manages input capabilities)
// -------------------------------------------------------------
static void seat_capabilities(void* data, struct wl_seat* seat, uint32_t capabilities) {
    (void)data;
    bool has_pointer = (capabilities & WL_SEAT_CAPABILITY_POINTER);
    bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD);

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
}

static void seat_name(void* data, struct wl_seat* seat, const char* name) {
    (void)data; (void)seat;
    std::cout << "[Seat] Seat name: " << (name ? name : "default") << std::endl;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

// -------------------------------------------------------------
// WAYLAND REGISTRY (Global Object Discovery)
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
// GAME CONTROLLER SUBSYSTEM (Linux /dev/input)
//
// WHY CONTROLLERS ARE NOT IN WAYLAND:
// Wayland only standardizes window management and desktop GUI input
// (mouse, keyboard, touchscreens, graphics tablets). Gamepads and
// joysticks deliberately BYPASS Wayland completely!
// On Linux, game engines access gamepads directly from the kernel via:
//   1. /dev/input/js* (Classic Linux Joystick API, <linux/joystick.h>)
//   2. /dev/input/event* (Modern evdev API, <linux/input.h>)
//
// Below is a zero-dependency implementation of the Linux Joystick API.
// -------------------------------------------------------------
void init_gamepad() {
    gamepad_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (gamepad_fd >= 0) {
        char name[128] = "Unknown Controller";
        if (ioctl(gamepad_fd, JSIOCGNAME(sizeof(name)), name) >= 0) {
            gamepad_name = name;
        }
        std::cout << "[Gamepad] Connected: " << gamepad_name << " (/dev/input/js0)" << std::endl;
    } else {
        std::cout << "[Gamepad] No controller found on /dev/input/js0 (plug one in anytime)" << std::endl;
    }
}

void poll_gamepad() {
    if (gamepad_fd < 0) {
        // Attempt hotplug reconnection periodically (every ~180 frames)
        static int reconnect_counter = 0;
        if (++reconnect_counter % 180 == 0) {
            gamepad_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
            if (gamepad_fd >= 0) {
                char name[128] = "Unknown Controller";
                if (ioctl(gamepad_fd, JSIOCGNAME(sizeof(name)), name) >= 0) gamepad_name = name;
                std::cout << "[Gamepad] Controller plugged in: " << gamepad_name << std::endl;
            }
        }
        return;
    }

    struct js_event e;
    while (read(gamepad_fd, &e, sizeof(e)) > 0) {
        uint8_t type = e.type & ~JS_EVENT_INIT; // Ignore startup state burst flag
        if (type == JS_EVENT_BUTTON) {
            std::cout << "[Gamepad] Button " << (int)e.number << ": "
                      << (e.value ? "PRESSED" : "RELEASED") << std::endl;
            if (e.value) {
                // Change triangle color based on button index
                tri_color[0] = (float)((e.number * 80 + 30) % 256) / 255.0f;
                tri_color[1] = (float)((e.number * 140 + 70) % 256) / 255.0f;
                tri_color[2] = (float)((e.number * 190 + 120) % 256) / 255.0f;
            }
        } else if (type == JS_EVENT_AXIS) {
            // Stick or trigger movement: e.value ranges from -32767 to +32767
            // Filter deadzone to prevent log flooding
            if (e.value > 8000 || e.value < -8000 || e.value == 0) {
                std::cout << "[Gamepad] Axis " << (int)e.number << ": " << e.value << std::endl;
            }
        }
    }

    if (errno == ENODEV) {
        std::cout << "[Gamepad] Controller disconnected!" << std::endl;
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

    if (wl_ptr) wl_pointer_destroy(wl_ptr);
    if (wl_kbd) wl_keyboard_destroy(wl_kbd);
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
    // 0. Initialize XKB context and Gamepad
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    init_gamepad();

    // 1. Core Wayland Connection
    wl_disp = wl_display_connect(nullptr);
    if (!wl_disp) {
        std::cerr << "Failed to connect to Wayland display." << std::endl;
        return -1;
    }

    struct wl_registry* registry = wl_display_get_registry(wl_disp);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    wl_display_roundtrip(wl_disp);

    if (!wl_comp || !xdg_wm) {
        std::cerr << "Required Wayland shells missing." << std::endl;
        return -1;
    }

    // 2. Establish Window Identity for Hyprland
    wl_surf = wl_compositor_create_surface(wl_comp);
    xdg_surf = xdg_wm_base_get_xdg_surface(xdg_wm, wl_surf);
    xdg_surface_add_listener(xdg_surf, &xdg_surface_listener, nullptr);
    
    xdg_top = xdg_surface_get_toplevel(xdg_surf);
    xdg_toplevel_add_listener(xdg_top, &xdg_toplevel_listener, nullptr);
    
    // Explicitly demand full screen mapping from Hyprland
    xdg_toplevel_set_fullscreen(xdg_top, nullptr); 
    wl_surface_commit(wl_surf);
    wl_display_roundtrip(wl_disp); // Enforce geometry creation

    // 3. EGL Device Pipeline Context Hooks
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

    // Default dimensions before fullscreen event auto-resizes
    wl_egl_win = wl_egl_window_create(wl_surf, 1920, 1080);
    egl_surface = eglCreateWindowSurface(egl_disp, egl_cfg, (EGLNativeWindowType)wl_egl_win, nullptr);
    eglMakeCurrent(egl_disp, egl_surface, egl_surface, egl_ctx);

    std::cout << "SUCCESS: OpenGL context successfully established on window!" << std::endl;

    // 4. Basic Modern OpenGL Object Initialization
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

    // Set up Triangle Coordinates
    float vertices[] = { -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f,  0.5f, 0.0f };
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 5. Main Non-Blocking Execution Loop
    while (running) {
        // Flush pending requests to Wayland compositor
        wl_display_flush(wl_disp);

        // Prepare non-blocking socket read
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

        // Poll gamepad input
        poll_gamepad();

        // OpenGL Render Passes
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Solid Black
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
