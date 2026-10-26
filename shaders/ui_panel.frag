#version 330 core

in vec2 vTexCoords;
in vec2 vLocalPixelPos;
in vec4 vColor;

out vec4 FragColor;

// Element dimensions
uniform vec2 uElementSize; // (width, height) in pixels

// Rounded corner radii: (x: top-left, y: top-right, z: bottom-right, w: bottom-left)
uniform vec4 uCornerRadii;

// Border
uniform float uBorderWidth; // Border thickness in pixels (0.0 for none)
uniform vec4  uBorderColor;

// Fill styling
uniform vec4  uFillColorTop;    // Top / primary fill color
uniform vec4  uFillColorBottom; // Bottom / secondary fill color (for vertical gradients)
uniform bool  uUseGradient;

// Texture / Sprite
uniform bool      uUseTexture;
uniform sampler2D uTexture;

// Drop shadow
uniform vec2  uShadowOffset; // (dx, dy) in pixels
uniform float uShadowBlur;   // Blur radius in pixels (0.0 = sharp shadow)
uniform vec4  uShadowColor;  // e.g. vec4(0.0, 0.0, 0.0, 0.4)

// Analytical SDF of a box with rounded corners
// p: point relative to center of box
// b: half-dimensions of box (size * 0.5)
// r: corner radius for the specific quadrant
float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    // Select corner radius based on quadrant:
    // x > 0, y < 0: top-right (r.y)
    // x < 0, y < 0: top-left  (r.x)
    // x > 0, y > 0: bottom-right (r.z)
    // x < 0, y > 0: bottom-left  (r.w)
    float rad = (p.x > 0.0) ? ((p.y > 0.0) ? r.z : r.y) : ((p.y > 0.0) ? r.w : r.x);
    rad = min(rad, min(b.x, b.y));

    vec2 q = abs(p) - b + vec2(rad);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rad;
}

void main() {
    vec2 halfSize = uElementSize * 0.5;
    vec2 p = vLocalPixelPos - halfSize; // Center origin

    // Distance to main box edge
    float dBox = sdRoundedBox(p, halfSize, uCornerRadii);
    float edgeAA = fwidth(dBox);
    edgeAA = max(edgeAA, 0.7);

    // Compute base fill color (solid, gradient, and/or textured)
    vec4 fill = uFillColorTop;
    if (uUseGradient) {
        float t = clamp(vLocalPixelPos.y / max(uElementSize.y, 1.0), 0.0, 1.0);
        fill = mix(uFillColorTop, uFillColorBottom, t);
    }
    fill *= vColor;

    if (uUseTexture) {
        vec4 texColor = texture(uTexture, vTexCoords);
        fill *= texColor;
    }

    // Interior alpha of the box
    float boxAlpha = 1.0 - smoothstep(-edgeAA, 0.0, dBox);

    // Border handling
    vec4 finalElementColor = fill;
    if (uBorderWidth > 0.0) {
        // Distance to inner border boundary
        float dInner = dBox + uBorderWidth;
        float borderAlpha = 1.0 - smoothstep(-edgeAA, 0.0, dInner);
        finalElementColor = mix(uBorderColor, fill, borderAlpha);
    }

    vec4 outColor = vec4(finalElementColor.rgb, finalElementColor.a * boxAlpha);

    // Drop shadow (rendered behind the box when shadow color has alpha > 0)
    if (uShadowColor.a > 0.001) {
        vec2 shadowP = p - uShadowOffset;
        float dShadow = sdRoundedBox(shadowP, halfSize, uCornerRadii);
        float blur = max(uShadowBlur, edgeAA);
        float shadowAlpha = 1.0 - smoothstep(-blur, blur, dShadow);
        vec4 shadow = vec4(uShadowColor.rgb, uShadowColor.a * shadowAlpha);

        // Blend element over shadow: out = elem + shadow * (1 - elem.a)
        outColor = vec4(
            outColor.rgb * outColor.a + shadow.rgb * shadow.a * (1.0 - outColor.a),
            outColor.a + shadow.a * (1.0 - outColor.a)
        );
    }

    if (outColor.a < 0.001) {
        discard;
    }

    FragColor = outColor;
}
