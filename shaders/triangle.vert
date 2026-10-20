#version 330 core

layout (location = 0) in vec3 aPos;

uniform vec2 uOffset;

void main() {
    gl_Position = vec4(aPos.x + uOffset.x, aPos.y + uOffset.y, aPos.z, 1.0);
}
