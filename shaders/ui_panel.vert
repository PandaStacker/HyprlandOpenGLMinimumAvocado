#version 330 core

layout (location = 0) in vec2 aPos;       // Local vertex position: [0.0, 0.0] to [1.0, 1.0] (or pixel coords)
layout (location = 1) in vec2 aTexCoords; // Texture coordinates [0.0, 1.0]
layout (location = 2) in vec4 aColor;     // Vertex color tint

out vec2 vTexCoords;
out vec2 vLocalPixelPos; // Pixel position relative to top-left of the element
out vec4 vColor;

// Orthographic projection matrix: maps (0, 0, width, height) to NDC (-1 to 1)
uniform mat4 uProjection;

// Element transform in screen pixels
uniform vec2 uElementPos;  // Screen position (x, y) in pixels
uniform vec2 uElementSize; // Dimensions (width, height) in pixels

void main() {
    // aPos is [0, 1] normalized quad coordinates
    vec2 pixelPos = uElementPos + aPos * uElementSize;
    vLocalPixelPos = aPos * uElementSize;
    vTexCoords = aTexCoords;
    vColor = aColor;

    gl_Position = uProjection * vec4(pixelPos, 0.0, 1.0);
}
