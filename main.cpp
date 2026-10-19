#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <iostream>
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
//    - Why: A dedicated server or multiplayer simulation worker must run without
//      a GPU or display server (Wayland/X11). Keeping AppState as pure C++ data
//      allows the exact same state and logic to run headless on a server.
//
// 2. PURE VALUE SEMANTICS (Trivially Copyable & Serializable):
//    - AppState is composed of plain standard data types with no pointers or deep graphs.
//    - Why: Multiplayer netcode relies on taking state snapshots and sending them
//      over the wire (UDP/TCP/WebSockets). Keeping state flat and self-contained
//      makes serialization, diffing, and syncing straightforward.
//
// 3. GRAPHICS AS A READ-ONLY OBSERVER:
//    - AppState represents the "ground truth" of the application.
//    - The graphics pipeline (OpenGL, shaders, window) is strictly an observer:
//      it reads AppState to draw frames, but AppState has zero awareness that
//      rendering or a window even exists.
// =============================================================================

struct AppState {
    float color[3] = { 1.0f, 0.5f, 0.2f }; // Default orange color
};

// Pure application logic: updates state based on input without touching OpenGL
void update_app(AppState& state, const InputMapper& input) {
    if (input.is_action_down(Action::ColorReset)) {
        state.color[0] = 1.0f; state.color[1] = 0.5f; state.color[2] = 0.2f; // Orange
    } else if (input.is_action_down(Action::ColorRed)) {
        state.color[0] = 1.0f; state.color[1] = 0.1f; state.color[2] = 0.1f; // Red
    } else if (input.is_action_down(Action::ColorGreen)) {
        state.color[0] = 0.1f; state.color[1] = 1.0f; state.color[2] = 0.1f; // Green
    } else if (input.is_action_down(Action::ColorBlue)) {
        state.color[0] = 0.1f; state.color[1] = 0.2f; state.color[2] = 1.0f; // Blue
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

    // 1. Application State (Can exist independently on a server without window/graphics)
    AppState app_state;

    // 2. Hardware Window & Graphics Setup (Client-only: completely ignored by a server)
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

    // Optional event hook: change state color on mouse clicks
    window.on_mouse_button([&](uint32_t button, bool pressed, double, double) {
        if (!pressed) return;
        if (button == BTN_LEFT)   { app_state.color[0] = 0.2f; app_state.color[1] = 0.8f; app_state.color[2] = 0.2f; } // Green
        if (button == BTN_RIGHT)  { app_state.color[0] = 0.2f; app_state.color[1] = 0.4f; app_state.color[2] = 1.0f; } // Blue
        if (button == BTN_MIDDLE) { app_state.color[0] = 1.0f; app_state.color[1] = 1.0f; app_state.color[2] = 0.2f; } // Yellow
    });

    // 3. Application Loop
    while (window.is_running()) {
        window.poll_events();

        // Step A: Update app state (Pure logic)
        update_app(app_state, window.input());

        // Step B: Graphics pass (Pure observer: reads app_state directly without interpolation)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        shader.set_vec3("uColor", app_state.color[0], app_state.color[1], app_state.color[2]);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        window.swap_buffers();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

    return 0;
}
