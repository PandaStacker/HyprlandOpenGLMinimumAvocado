#version 330 core

layout (location = 0) in vec2 aPos;       // Position in screen pixels
layout (location = 1) in vec2 aTexCoords; // Texture coordinates on font atlas
layout (location = 2) in vec4 aColor;     // Vertex color

out vec2 vTexCoords;
out vec4 vColor;

uniform mat4 uProjection;

void main() {
    vTexCoords = aTexCoords;
    vColor = aColor;

    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
