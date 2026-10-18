// Compile with: g++ main.cpp -x c xdg-shell-protocol.c -o full_gl -lwayland-client -lwayland-egl -lEGL -lGL
#define GL_GLEXT_PROTOTYPES
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <cstring>
#include <iostream>

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

EGLDisplay             egl_disp     = EGL_NO_DISPLAY;
EGLContext             egl_ctx      = EGL_NO_CONTEXT;
EGLSurface             egl_surface  = EGL_NO_SURFACE;

// Basic Shader Source Code (Modern Programmable OpenGL)
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); }\0";
const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main() { FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f); }\0"; // Orange

// Protocol Event Handlers
static void xdg_toplevel_configure(void* data, struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states) {
    if (width > 0 && height > 0 && wl_egl_win) {
        wl_egl_window_resize(wl_egl_win, width, height, 0, 0);
        glViewport(0, 0, width, height);
    }
}
static void xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) { exit(0); }
static const struct xdg_toplevel_listener xdg_toplevel_listener = { xdg_toplevel_configure, xdg_toplevel_close };

static void xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial) {
    xdg_surface_ack_configure(surface, serial); // Handshake complete
}
static const struct xdg_surface_listener xdg_surface_listener = { xdg_surface_configure };

static void wm_base_ping(void* data, struct xdg_wm_base* wm, uint32_t serial) {
    xdg_wm_base_pong(wm, serial); // Tell Hyprland we haven't crashed
}
static const struct xdg_wm_base_listener wm_base_listener = { wm_base_ping };

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
    if (std::strcmp(interface, "wl_compositor") == 0) {
        wl_comp = (struct wl_compositor*)wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
        xdg_wm = (struct xdg_wm_base*)wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm, &wm_base_listener, nullptr);
    }
}
static void registry_handle_global_remove(void* data, struct wl_registry* registry, uint32_t id) {}
static const struct wl_registry_listener registry_listener = { registry_handle_global, registry_handle_global_remove };

int main() {
    // 1. Core Connection
    wl_disp = wl_display_connect(nullptr);
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

    EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
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

    // Clean shaders post-link
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Set up Triangle Coordinates
    float vertices[] = { -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f,  0.5f, 0.0f };
    GLuint VBO, VAO;
    // Note: If core profile requires glGenVertexArrays, you must generate a VAO container 
    typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
    typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint);
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)eglGetProcAddress("glGenVertexArrays");
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)eglGetProcAddress("glBindVertexArray");

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 5. Main Execution Loop
    while (true) {
        wl_display_dispatch_pending(wl_disp);

        // Core Modern OpenGL Render Passes
        glClearColor(0.065f, 0.5f, 0.3f, 1.0f); // Solid Black
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        eglSwapBuffers(egl_disp, egl_surface);
    }
    return 0;
}

