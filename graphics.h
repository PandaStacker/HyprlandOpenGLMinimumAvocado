#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <memory>
#include <cstdint>
#include "app_state.h"

class Shader;

// =============================================================================
// Graphics Subsystem
// =============================================================================
//
// Strictly responsible for rendering and GPU resources.
// Graphics acts as a pure, read-only observer of AppState.
// A dedicated server or headless simulation never instantiates this class.
// =============================================================================
class Graphics {
public:
    Graphics();
    ~Graphics();

    // Non-copyable, movable
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;
    Graphics(Graphics&&) noexcept;
    Graphics& operator=(Graphics&&) noexcept;

    // Hot-reload shaders
    void reload_shaders();

    // Render pass (Reads AppState as a pure observer)
    void render(const AppState& state);

private:
    uint32_t vao_{0};
    uint32_t vbo_{0};
    std::unique_ptr<Shader> shader_;
};

#endif // GRAPHICS_H
