#ifndef APP_H
#define APP_H

#include <memory>
#include "wayland_window.h"
#include "graphics.h"
#include "local_sim_synchronizer.h"

// =============================================================================
// Application Harness
// =============================================================================
//
// Encapsulates configuration, window lifecycle, and orchestrates the loop:
//   Input -> LocalSim::update() -> Graphics::render() -> swap_buffers().
//
// -----------------------------------------------------------------------------
// HOW TO SWAP OUT THE WINDOW CLASS:
// -----------------------------------------------------------------------------
// The App class couples to WaylandWindow only through a minimal interface contract:
//   1. window_.is_running()         -> bool
//   2. window_.poll_events()        -> void
//   3. window_.swap_buffers()       -> void
//   4. window_.input()              -> InputMapper&
//   5. window_.on_key(...)          -> void (optional hook)
//   6. window_.on_mouse_button(...) -> void (optional hook)
//
// To replace WaylandWindow (e.g. with GLFW, SDL3, an X11 backend, or a headless stub):
//   - OPTION 1 (Direct Substitution): Replace `WaylandWindow window_;` in this class
//     with your custom window class providing the methods above.
//   - OPTION 2 (Polymorphic Interface): Introduce an `IWindow` interface and store
//     `std::unique_ptr<IWindow> window_;` to allow runtime switching between
//     Wayland, X11, and Headless modes.
//   - OPTION 3 (Headless Server / Dedicated Worker): Since LocalSim and simulation
//     logic have zero window or graphics dependencies, a headless server can bypass
//     App, WaylandWindow, and Graphics entirely.
// =============================================================================
class App {
public:
    // Default constructor or command-line constructor
    explicit App(int argc = 0, char** argv = nullptr);

    // Constructor supporting a custom or user-derived LocalSim
    explicit App(std::unique_ptr<LocalSim> custom_state, int argc = 0, char** argv = nullptr);

    ~App() = default;

    // Runs the main application loop
    int run();

    // Accessors
    [[nodiscard]] WaylandWindow& window() noexcept { return window_; }
    [[nodiscard]] Graphics& graphics() noexcept { return graphics_; }
    [[nodiscard]] LocalSimSynchronizer& state() noexcept { return *local_sim_; }

private:
    static WindowConfig parse_config(int argc, char** argv);
    void init_event_hooks();

    // Note: window_ must be declared BEFORE graphics_ so the OpenGL context
    // is established before Graphics allocates OpenGL buffers and shaders.
    WaylandWindow             window_;
    Graphics                  graphics_;
    LocalSimSynchronizer local_sim_synchronizer_;
};

#endif // APP_H
