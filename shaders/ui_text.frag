#version 330 core

in vec2 vTexCoords;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uFontAtlas;      // Single-channel SDF font atlas

// Text styling
uniform vec4  uTextColor;          // Font color tint
uniform float uSDFThreshold;       // Base distance threshold (default: 0.5)

// Outline / halo
uniform bool  uUseOutline;
uniform vec4  uOutlineColor;       // Outline color
uniform float uOutlineWidth;       // Outline thickness [0.0, 0.4]

// Drop Shadow
uniform bool  uUseShadow;
uniform vec2  uShadowOffset;       // Shadow UV offset on atlas or pixel offset
uniform vec4  uShadowColor;
uniform float uShadowBlur;

void main() {
    float threshold = (uSDFThreshold > 0.0) ? uSDFThreshold : 0.5;

    // Sample distance from font atlas
    float dist = texture(uFontAtlas, vTexCoords).r;
    float edgeWidth = fwidth(dist);

    // Primary text alpha with subpixel anti-aliasing
    float textAlpha = smoothstep(threshold - edgeWidth, threshold + edgeWidth, dist);
    vec4 textCol = uTextColor * vColor;
    textCol.a *= textAlpha;

    vec4 finalColor = textCol;

    // Outline / halo effect
    if (uUseOutline && uOutlineWidth > 0.0) {
        float outlineThresh = max(threshold - uOutlineWidth, 0.0);
        float outlineAlpha = smoothstep(outlineThresh - edgeWidth, outlineThresh + edgeWidth, dist);
        vec4 outCol = uOutlineColor;
        outCol.a *= outlineAlpha;

        // Composite text over outline
        finalColor = mix(outCol, textCol, textAlpha);
    }

    // Drop shadow
    if (uUseShadow && uShadowColor.a > 0.001) {
        float shadowDist = texture(uFontAtlas, vTexCoords - uShadowOffset).r;
        float shadowEdge = edgeWidth + uShadowBlur;
        float shadowAlpha = smoothstep(threshold - shadowEdge, threshold + shadowEdge, shadowDist);
        vec4 shadCol = uShadowColor;
        shadCol.a *= shadowAlpha;

        // Composite text/outline over shadow
        finalColor = vec4(
            finalColor.rgb * finalColor.a + shadCol.rgb * shadCol.a * (1.0 - finalColor.a),
            finalColor.a + shadCol.a * (1.0 - finalColor.a)
        );
    }

    if (finalColor.a < 0.001) {
        discard;
    }

    FragColor = finalColor;
}
