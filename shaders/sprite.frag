#version 330 core

in vec2 vTexCoords;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uSpriteTexture;

// Alpha testing / cutout
uniform float uAlphaCutoff; // Discards fragments below this alpha (default: 0.01)

// Sprite Effects (hit flash, desaturation)
uniform vec3  uFlashColor;  // e.g. vec3(1.0, 1.0, 1.0) for damage flash
uniform float uFlashAmount; // 0.0 (normal) to 1.0 (pure flash color)
uniform float uDesaturate;  // 0.0 (normal) to 1.0 (grayscale)

void main() {
    vec4 texColor = texture(uSpriteTexture, vTexCoords);
    vec4 finalColor = texColor * vColor;

    // Discard transparent cutout areas
    float cutoff = (uAlphaCutoff > 0.0) ? uAlphaCutoff : 0.01;
    if (finalColor.a < cutoff) {
        discard;
    }

    // Optional grayscale desaturation
    if (uDesaturate > 0.0) {
        float gray = dot(finalColor.rgb, vec3(0.299, 0.587, 0.114));
        finalColor.rgb = mix(finalColor.rgb, vec3(gray), clamp(uDesaturate, 0.0, 1.0));
    }

    // Optional damage / hit flash
    if (uFlashAmount > 0.0) {
        finalColor.rgb = mix(finalColor.rgb, uFlashColor, clamp(uFlashAmount, 0.0, 1.0));
    }

    FragColor = finalColor;
}
