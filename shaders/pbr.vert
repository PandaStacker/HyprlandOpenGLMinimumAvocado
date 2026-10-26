#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

out vec3 vWorldPos;
out vec2 vTexCoords;
out vec3 vNormal;
out mat3 vTBN;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vTexCoords = aTexCoords;

    // Normal mapping basis (TBN) using Gram-Schmidt process
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(uNormalMatrix * aNormal);
    // Re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;

    gl_Position = uProjection * uView * worldPos;
}
