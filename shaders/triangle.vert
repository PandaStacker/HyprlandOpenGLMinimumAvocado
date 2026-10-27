#version 450

layout (location = 0) in vec3 aPos;

layout(push_constant) uniform PushConstants {
    vec2 uOffset;
    vec3 uColor;
} pc;

void main() {
    gl_Position = vec4(aPos.x + pc.uOffset.x, aPos.y + pc.uOffset.y, aPos.z, 1.0);
}
