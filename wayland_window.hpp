#ifndef WAYLAND_WINDOW_H
#define WAYLAND_WINDOW_H

#include <string>
#include <functional>
#include <memory>
#include <cstdint>
#include "input_mapper.hpp"

// Forward declaration of internal PIMPL implementation.
// Isolates consumer code from Wayland C protocol headers and structs.
class WaylandWindowImpl;

struct WindowConfig {
    // Window settings
    int         width              = 1280;
    int         height             = 720;
    std::string title              = "Wayland Vulkan Window";
    std::string app_id             = "wayland.minimums";
    bool        fullscreen         = false;
    bool        enable_exit_button = false; // If true, '_' will quit the application
    bool        vsync              = true;  // Vulkan present mode: FIFO vs MAILBOX / IMMEDIATE
};

class WaylandWindow {
public:
    explicit WaylandWindow(const WindowConfig& config = WindowConfig{});
    WaylandWindow(int width, int height, const std::string& title = "Wayland Vulkan Window", bool fullscreen = false);
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
    void swap_buffers(); // Advances frame state & input lifecycle
    void set_vsync(bool enabled);

    // --- Input Subsystem ---
    [[nodiscard]] InputMapper& input() noexcept;
    [[nodiscard]] const InputMapper& input() const noexcept;

    // --- Window State Queries & Controls ---
    [[nodiscard]] const WindowConfig& get_config() const noexcept;
    [[nodiscard]] int get_width() const noexcept;
    [[nodiscard]] int get_height() const noexcept;
    [[nodiscard]] int get_pixel_width() const noexcept;
    [[nodiscard]] int get_pixel_height() const noexcept;
    [[nodiscard]] double get_scale_factor() const noexcept;
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

    // --- Low-Level Wayland Handles (For Vulkan VK_KHR_wayland_surface) ---
    [[nodiscard]] void* get_wl_display() const noexcept;
    [[nodiscard]] void* get_wl_surface() const noexcept;

private:
    std::unique_ptr<WaylandWindowImpl> impl;
};

#endif // WAYLAND_WINDOW_H
