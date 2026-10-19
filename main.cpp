#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <iostream>
#include <chrono>
#include "wayland_window.h"
#include "shader.h"

// =============================================================================
// ARCHITECTURE & MULTIPLAYER DESIGN: AppState
// =============================================================================
//
// Why does AppState look like this?
//
// 1. ZERO GRAPHICS / PLATFORM DEPENDENCIES (Headless Server Ready):
//    - AppState contains NO OpenGL identifiers (VAOs, VBOs, textures), NO Wayland
//      window pointers, and NO display-server headers.
//    - Why: In a multiplayer game, a dedicated server or headless simulation worker
//      must run without a GPU or display server (Wayland/X11). Keeping AppState as
//      pure standard C++ data allows the exact same simulation code to compile and
//      run on both a headless Linux server and a graphical client.
//
// 2. PURE VALUE SEMANTICS (Trivially Copyable & Serializable):
//    - AppState is a plain struct of value types with no pointers or deep graphs.
//    - Why: Multiplayer netcode fundamentally requires taking state snapshots.
//      Keeping state trivially copyable allows:
//        a) Network serialization (sending state snapshots/deltas over UDP/WebSockets).
//        b) Snapshot history buffers (saving previous ticks for render interpolation).
//        c) Client-side prediction & rollback (restoring past ticks upon receiving
//           authoritative server updates).
//
// 3. DISCRETE, FIXED-TIMESTEP TICK SIMULATION:
//    - AppState tracks a discrete monotonic 'tick' number, incremented only by fixed
//      simulation steps (e.g. 60Hz), completely decoupled from display refresh rates.
//    - Why: Clients run on varying refresh rates (60Hz, 144Hz, 240Hz, or VRR). If game
//      logic ran on variable display frame times, simulations on different machines
//      would immediately desynchronize.
//
// 4. GRAPHICS AS AN OBSERVER (Decoupled Representation):
//    - Graphics and windowing code are strictly "read-only observers". They consume
//      AppState (interpolating between the previous tick and current tick via 'alpha')
//      to draw frames, but NEVER directly mutate AppState during the render pass.
// =============================================================================

struct AppState {
    uint64_t tick = 0;                      // Monotonic simulation step counter
    float    color[3] = { 1.0f, 0.5f, 0.2f }; // Default orange color
};

enum class ColorCommand : uint8_t {
    None = 0,
    Reset,
    Red,
    Green,
    Blue,
    Yellow
};

// Represents player/client intent sampled for a simulation tick
struct AppInput {
    ColorCommand color_cmd = ColorCommand::None;
};

// Pure, deterministic simulation step: mutates AppState based strictly on input and dt
void simulate(AppState& state, const AppInput& input, double /*dt*/) {
    state.tick++;

    switch (input.color_cmd) {
        case ColorCommand::Reset:
            state.color[0] = 1.0f; state.color[1] = 0.5f; state.color[2] = 0.2f; // Orange
            break;
        case ColorCommand::Red:
            state.color[0] = 1.0f; state.color[1] = 0.1f; state.color[2] = 0.1f; // Red
            break;
        case ColorCommand::Green:
            state.color[0] = 0.2f; state.color[1] = 0.8f; state.color[2] = 0.2f; // Green
            break;
        case ColorCommand::Blue:
            state.color[0] = 0.2f; state.color[1] = 0.4f; state.color[2] = 1.0f; // Blue
            break;
        case ColorCommand::Yellow:
            state.color[0] = 1.0f; state.color[1] = 1.0f; state.color[2] = 0.2f; // Yellow
            break;
        case ColorCommand::None:
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// Application Entry Point
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    bool enable_exit = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--enable-exit" || arg == "-e" || arg == "--exit-key" || arg == "--test") {
            enable_exit = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --enable-exit, -e   Enable '_' exit button (disabled by default)\n"
                      << "  --help, -h          Show this help message\n";
            return 0;
        }
    }

    // 1. Instantiate the window object
    WaylandWindow window(WindowConfig{
        .width              = 1920,
        .height             = 1200,
        .title              = "Wayland Minimums - Zero Framework OpenGL",
        .fullscreen         = true,
        .enable_exit_button = enable_exit
    });

    if (enable_exit) {
        std::cout << "[Config] Exit button ('_') is ENABLED." << std::endl;
    } else {
        std::cout << "[Config] Exit button ('_') is DISABLED by default (run with --enable-exit to test)." << std::endl;
    }

    // 2. Pure OpenGL setup
    Shader shader("shaders/triangle.vert", "shaders/triangle.frag");

    // Optional event hook: Press F5 to hot-reload shaders during runtime
    window.on_key([&](uint32_t, uint32_t keysym, bool pressed) {
        if (pressed && keysym == XKB_KEY_F5) {
            shader.reload();
        }
    });

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Track asynchronous mouse click intent to feed into the fixed-tick input buffer
    ColorCommand pending_mouse_cmd = ColorCommand::None;
    window.on_mouse_button([&](uint32_t button, bool pressed, double, double) {
        if (!pressed) return;
        if (button == BTN_LEFT)   pending_mouse_cmd = ColorCommand::Green;
        if (button == BTN_RIGHT)  pending_mouse_cmd = ColorCommand::Blue;
        if (button == BTN_MIDDLE) pending_mouse_cmd = ColorCommand::Yellow;
    });

    // 3. Application State & Simulation Loop Setup
    AppState current_state;
    AppState previous_state = current_state;

    constexpr double TICK_RATE = 1.0 / 60.0; // 60 Hz fixed simulation step
    double accumulator = 0.0;
    auto last_time = std::chrono::steady_clock::now();

    // 4. Decoupled Render & Fixed-Tick Loop
    while (window.is_running()) {
        window.poll_events();

        auto now = std::chrono::steady_clock::now();
        double frame_time = std::chrono::duration<double>(now - last_time).count();
        last_time = now;

        // Clamp frame time to prevent accumulator explosion if paused / dragging window
        if (frame_time > 0.25) {
            frame_time = 0.25;
        }
        accumulator += frame_time;

        // Sample input commands for the simulation ticks
        auto& input_map = window.input();
        AppInput input{};

        if (pending_mouse_cmd != ColorCommand::None) {
            input.color_cmd = pending_mouse_cmd;
            pending_mouse_cmd = ColorCommand::None;
        } else if (input_map.is_action_down(Action::ColorReset)) {
            input.color_cmd = ColorCommand::Reset;
        } else if (input_map.is_action_down(Action::ColorRed)) {
            input.color_cmd = ColorCommand::Red;
        } else if (input_map.is_action_down(Action::ColorGreen)) {
            input.color_cmd = ColorCommand::Green;
        } else if (input_map.is_action_down(Action::ColorBlue)) {
            input.color_cmd = ColorCommand::Blue;
        }

        // Fixed-timestep simulation: advances state in deterministic intervals
        while (accumulator >= TICK_RATE) {
            previous_state = current_state;
            simulate(current_state, input, TICK_RATE);
            accumulator -= TICK_RATE;
        }

        // Interpolation factor: [0.0, 1.0] representing time between prev and curr ticks
        float alpha = static_cast<float>(accumulator / TICK_RATE);

        // Graphics pass: pure observer reading interpolated state
        float render_color[3];
        for (int i = 0; i < 3; ++i) {
            render_color[i] = previous_state.color[i] * (1.0f - alpha) + current_state.color[i] * alpha;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        shader.set_vec3("uColor", render_color[0], render_color[1], render_color[2]);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        window.swap_buffers();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

    return 0;
}
