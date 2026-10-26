#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

out vec3 vWorldPos;
out vec2 vTexCoords;
out vec3 vNormal;
out mat3 vTBN;
out vec3 vViewDir;

// Matrices
uniform mat4 uModel;               // Weapon / viewmodel local transform
uniform mat4 uSwayMatrix;          // Procedural sway & bobbing matrix
uniform mat4 uView;                // Camera view matrix
uniform mat4 uViewmodelProjection; // Dedicated viewmodel projection (custom FOV)
uniform mat3 uNormalMatrix;        // Normal matrix

// Depth compression to prevent first-person weapon from clipping through world walls
// Setting uDepthScale < 1.0 (e.g. 0.2) compresses the weapon depth into the near clip range.
uniform float uDepthScale;         // Default: 0.2
uniform float uDepthBias;          // Default: 0.0

void main() {
    // Apply sway and model transformations
    vec4 worldPos = uModel * uSwayMatrix * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vTexCoords = aTexCoords;

    // TBN construction
    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 N = normalize(uNormalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;

    // View direction in world space
    // For viewmodels, camera is origin of view space
    vec4 viewPos = uView * worldPos;
    vViewDir = -normalize(viewPos.xyz);

    // Project viewmodel
    vec4 clipPos = uViewmodelProjection * viewPos;

    // Depth compression: squashes NDC Z closer to the near plane (-1.0 to 1.0)
    // to prevent weapon clipping through walls while preserving relative depth sorting of the gun parts.
    float scale = (uDepthScale > 0.0) ? uDepthScale : 1.0;
    clipPos.z = clipPos.z * scale + uDepthBias * clipPos.w;

    gl_Position = clipPos;
}
