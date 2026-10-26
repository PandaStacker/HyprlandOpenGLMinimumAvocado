#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 vTexCoords;

uniform mat4 uProjection;
uniform mat4 uView;

void main() {
    vTexCoords = aPos;

    // Remove camera translation so skybox is always at infinity relative to the player
    mat4 viewNoTranslation = mat4(mat3(uView));
    vec4 pos = uProjection * viewNoTranslation * vec4(aPos, 1.0);

    // xyww ensures depth is always 1.0 (the maximum depth / far plane) in NDC after perspective divide
    gl_Position = pos.xyww;
}
