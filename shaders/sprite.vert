#version 330 core

layout (location = 0) in vec3 aPos;       // Quad position centered at origin [-0.5, 0.5]
layout (location = 1) in vec2 aTexCoords; // Base UV coordinates [0.0, 1.0]

out vec2 vTexCoords;
out vec4 vColor;

// Matrices
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

// Sprite sheet / atlas UV animation
uniform vec2 uUVOffset; // Offset in atlas (e.g. frame_x / atlas_cols)
uniform vec2 uUVScale;  // Scale in atlas (e.g. 1.0 / atlas_cols)

// Sprite flipping
uniform bool uFlipX;
uniform bool uFlipY;

// Billboarding in 3D world space (for 2.5D characters, particles, foliage)
uniform bool uBillboard3D;           // Spherical billboard: always faces camera directly
uniform bool uBillboardCylindrical;  // Cylindrical billboard: rotates only on Y axis (trees/pillars)
uniform vec2 uSpriteScale;           // Width and height scale in world units (default: vec2(1.0))

// Color tint
uniform vec4 uColor;                 // Multiplier color (default: vec4(1.0))

void main() {
    // 1. Calculate UV coordinates with atlas framing and optional flipping
    vec2 uv = aTexCoords;
    if (uFlipX) uv.x = 1.0 - uv.x;
    if (uFlipY) uv.y = 1.0 - uv.y;

    vec2 scale = (uUVScale.x != 0.0 && uUVScale.y != 0.0) ? uUVScale : vec2(1.0);
    vTexCoords = uUVOffset + uv * scale;
    vColor = uColor;

    // 2. Position calculation
    vec2 spriteSize = (uSpriteScale.x != 0.0 && uSpriteScale.y != 0.0) ? uSpriteScale : vec2(1.0);

    if (uBillboard3D) {
        // World position of sprite origin
        vec4 worldOrigin = uModel * vec4(0.0, 0.0, 0.0, 1.0);

        if (uBillboardCylindrical) {
            // Cylindrical billboarding: preserve world Y axis, face camera in XZ plane
            vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
            cameraRight.y = 0.0;
            cameraRight = normalize(cameraRight);

            vec3 worldUp = vec3(0.0, 1.0, 0.0);
            vec3 worldPos = worldOrigin.xyz 
                          + cameraRight * (aPos.x * spriteSize.x)
                          + worldUp * (aPos.y * spriteSize.y);

            gl_Position = uProjection * uView * vec4(worldPos, 1.0);
        } else {
            // Spherical billboarding: align quad directly in view space
            vec4 viewOrigin = uView * worldOrigin;
            vec4 viewPos = viewOrigin + vec4(aPos.x * spriteSize.x, aPos.y * spriteSize.y, 0.0, 0.0);

            gl_Position = uProjection * viewPos;
        }
    } else {
        // Standard 2D orthographic or 3D world planar sprite
        gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    }
}
