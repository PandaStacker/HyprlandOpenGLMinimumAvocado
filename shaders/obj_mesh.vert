#version 330 core

// -----------------------------------------------------------------------------
// OBJ Mesh Vertex Shader
// Accepts standard Wavefront OBJ vertex attributes (v, vn, vt, tangents)
// -----------------------------------------------------------------------------

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

out vec3 vWorldPos;
out vec2 vTexCoords;
out vec3 vNormal;
out mat3 vTBN;

// Transformation matrices
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

// UV tiling and offset controls for materials
uniform vec2 uUVTiling; // Texture repeat (default: vec2(1.0, 1.0))
uniform vec2 uUVOffset; // Texture shift (default: vec2(0.0, 0.0))

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // Apply material UV scale/offset
    vec2 tiling = (uUVTiling.x != 0.0 && uUVTiling.y != 0.0) ? uUVTiling : vec2(1.0);
    vTexCoords = aTexCoords * tiling + uUVOffset;

    // Normal mapping basis (TBN)
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(uNormalMatrix * aNormal);
    // Gram-Schmidt re-orthogonalization
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;

    gl_Position = uProjection * uView * worldPos;
}
