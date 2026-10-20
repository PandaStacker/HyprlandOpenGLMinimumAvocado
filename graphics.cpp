#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include "graphics.h"
#include "shader.h"

Graphics::Graphics() {
    shader_ = std::make_unique<Shader>("shaders/triangle.vert", "shaders/triangle.frag");

    // Sized triangle geometry
    float vertices[] = {
        -0.2f, -0.2f, 0.0f,
         0.2f, -0.2f, 0.0f,
         0.0f,  0.2f, 0.0f
    };

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

Graphics::~Graphics() {
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
}

Graphics::Graphics(Graphics&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), shader_(std::move(other.shader_)) {
    other.vao_ = 0;
    other.vbo_ = 0;
}

Graphics& Graphics::operator=(Graphics&& other) noexcept {
    if (this != &other) {
        if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
        if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        shader_ = std::move(other.shader_);
        other.vao_ = 0;
        other.vbo_ = 0;
    }
    return *this;
}

void Graphics::reload_shaders() {
    if (shader_) {
        shader_->reload();
    }
}

void Graphics::render(const AppState& state) {
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!shader_) return;

    // As this becomes more advanced, it continues to do the same thing, it reads
    // and interprets the app state visually. Apps may have lots of 2D and 3D things
    // to say, they'll be loaded up in a graphics loading class, then applied here
    // to the app state if they're the client.
    shader_->use();
    shader_->set_vec2("uOffset", state.x, state.y);
    shader_->set_vec3("uColor", state.color[0], state.color[1], state.color[2]);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
