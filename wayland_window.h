#ifndef WAYLAND_WINDOW_H
#define WAYLAND_WINDOW_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include "input_mapper.h"

// Forward declaration of internal PIMPL implementation.
// This completely isolates consumer code from Wayland C protocol headers and structs.
class WaylandWindowImpl;

struct WindowConfig {
    // Window settings
    int         width              = 1280;
    int         height             = 720;
    std::string title              = "Wayland Window";
    std::string app_id             = "wayland.minimums";
    bool        fullscreen         = false;
    bool        enable_exit_button = false; // If true, '_' will quit the application

    // OpenGL & Framebuffer settings (configured without exposing EGL/Wayland)
    int         gl_major           = 3;     // Target OpenGL major version (e.g. 3 or 4)
    int         gl_minor           = 3;     // Target OpenGL minor version (e.g. 3 for 3.3)
    bool        gl_core_profile    = true;  // Request Core profile (vs Compatibility)
    int         depth_bits         = 24;    // Depth buffer bits (e.g. 24 for 3D depth, 0 to disable)
    int         stencil_bits       = 8;     // Stencil buffer bits (e.g. 8 for shadows/masks, 0 to disable)
    int         samples            = 0;     // Multisampling (MSAA) samples: 0, 2, 4, 8...
    int         red_bits           = 8;
    int         green_bits         = 8;
    int         blue_bits          = 8;
    int         alpha_bits         = 8;
    bool        vsync              = true;  // Synchronize buffer swaps to display refresh rate
};

class WaylandWindow {
public:
    explicit WaylandWindow(const WindowConfig& config = WindowConfig{});
    WaylandWindow(int width, int height, const std::string& title = "Wayland Window", bool fullscreen = false);
    ~WaylandWindow();

    // Non-copyable, movable
    WaylandWindow(const WaylandWindow&) = delete;
    WaylandWindow& operator=(const WaylandWindow&) = delete;
    WaylandWindow(WaylandWindow&&) noexcept;
    WaylandWindow& operator=(WaylandWindow&&) noexcept;

    // --- Core Window & Event Lifecycle ---
    [[nodiscard]] bool is_running() const noexcept;
    void close() noexcept;
    void poll_events();
    void swap_buffers();
    void set_vsync(bool enabled);

    // --- Input Subsystem ---
    [[nodiscard]] InputMapper& input() noexcept;
    [[nodiscard]] const InputMapper& input() const noexcept;

    // --- Window State Queries & Controls ---
    [[nodiscard]] const WindowConfig& get_config() const noexcept;
    [[nodiscard]] int get_width() const noexcept;
    [[nodiscard]] int get_height() const noexcept;
    [[nodiscard]] bool is_fullscreen() const noexcept;
    void set_fullscreen(bool fullscreen);
    void set_title(const std::string& title);

    // --- Optional Event Callbacks ---
    using ResizeCallback      = std::function<void(int width, int height)>;
    using KeyCallback         = std::function<void(uint32_t scancode, uint32_t keysym, bool pressed)>;
    using MouseButtonCallback = std::function<void(uint32_t button, bool pressed, double x, double y)>;
    using MouseMotionCallback = std::function<void(double x, double y)>;
    using TouchCallback       = std::function<void(int32_t id, double x, double y, bool down)>;
    using StylusCallback      = std::function<void(double x, double y, double pressure, double tilt_x, double tilt_y)>;

    void on_resize(ResizeCallback cb);
    void on_key(KeyCallback cb);
    void on_mouse_button(MouseButtonCallback cb);
    void on_mouse_motion(MouseMotionCallback cb);
    void on_touch(TouchCallback cb);
    void on_stylus(StylusCallback cb);

    // --- Low-Level Handles (for advanced Vulkan / custom EGL interop) ---
    [[nodiscard]] void* get_wl_display() const noexcept;
    [[nodiscard]] void* get_wl_surface() const noexcept;
    [[nodiscard]] void* get_egl_display() const noexcept;
    [[nodiscard]] void* get_egl_context() const noexcept;

private:
    std::unique_ptr<WaylandWindowImpl> impl;
};

#endif // WAYLAND_WINDOW_H
