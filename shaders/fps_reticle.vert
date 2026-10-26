#version 330 core

// Simple quad vertex shader for screen-centered reticle / crosshair
layout (location = 0) in vec2 aPos; // Normalized [-1.0, 1.0]

out vec2 vScreenPos;

uniform vec2 uViewportSize; // Window resolution in pixels (width, height)
uniform vec2 uReticleOffset; // Center offset in pixels (for weapon sway / aim offset)

void main() {
    // Convert NDC offset to pixel coordinates passed to fragment shader
    vec2 pixelPos = aPos * (uViewportSize * 0.5);
    vScreenPos = pixelPos - uReticleOffset;

    gl_Position = vec4(aPos, 0.0, 1.0);
}
