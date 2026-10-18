#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <iostream>
#include "wayland_window.h"

// -----------------------------------------------------------------------------
// Shaders & Geometry for Visual OpenGL Verification
// -----------------------------------------------------------------------------
static const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); }\n";

static const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uColor;\n"
    "void main() { FragColor = vec4(uColor, 1.0f); }\n";

static GLuint create_shader_program() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
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

    // 1. Instantiate the window object — that's it!
    // Hardware accelerated OpenGL context, XDG Shell, and input routing are established automatically.
    WaylandWindow window(WindowConfig{
        .width              = 1920,
        .height             = 1080,
        .title              = "Wayland Minimums - Zero Framework OpenGL",
        .fullscreen         = true,
        .enable_exit_button = enable_exit
    });

    if (enable_exit) {
        std::cout << "[Config] Exit button ('_') is ENABLED." << std::endl;
    } else {
        std::cout << "[Config] Exit button ('_') is DISABLED by default (run with --enable-exit to test)." << std::endl;
    }

    // Example: Custom input remapping (commented out)
    // Map keyboard 'F' key to Action::SecondaryAction:
    // window.input().bind_action(Action::SecondaryAction, InputDevice::Keyboard, XKB_KEY_f);

    // 2. Pure OpenGL setup — focus entirely on your rendering logic
    GLuint shaderProgram = create_shader_program();
    GLint  uColorLoc      = glGetUniformLocation(shaderProgram, "uColor");

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

    float triangle_color[3] = { 1.0f, 0.5f, 0.2f }; // Default orange

    // Optional event hook: change color on mouse clicks
    window.on_mouse_button([&](uint32_t button, bool pressed, double, double) {
        if (!pressed) return;
        if (button == BTN_LEFT)   { triangle_color[0] = 0.2f; triangle_color[1] = 0.8f; triangle_color[2] = 0.2f; } // Green
        if (button == BTN_RIGHT)  { triangle_color[0] = 0.2f; triangle_color[1] = 0.4f; triangle_color[2] = 1.0f; } // Blue
        if (button == BTN_MIDDLE) { triangle_color[0] = 1.0f; triangle_color[1] = 1.0f; triangle_color[2] = 0.2f; } // Yellow
    });

    // 3. Simple and clean application render loop
    while (window.is_running()) {
        window.poll_events();

        // Check virtual actions via InputMapper
        auto& input = window.input();
        if (input.is_action_down(Action::ColorReset)) {
            triangle_color[0] = 1.0f; triangle_color[1] = 0.5f; triangle_color[2] = 0.2f; // Orange
        } else if (input.is_action_down(Action::ColorRed)) {
            triangle_color[0] = 1.0f; triangle_color[1] = 0.1f; triangle_color[2] = 0.1f; // Red
        } else if (input.is_action_down(Action::ColorGreen)) {
            triangle_color[0] = 0.1f; triangle_color[1] = 1.0f; triangle_color[2] = 0.1f; // Green
        } else if (input.is_action_down(Action::ColorBlue)) {
            triangle_color[0] = 0.1f; triangle_color[1] = 0.2f; triangle_color[2] = 1.0f; // Blue
        }

        // OpenGL rendering pass
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform3f(uColorLoc, triangle_color[0], triangle_color[1], triangle_color[2]);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        window.swap_buffers();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);

    return 0;
}
