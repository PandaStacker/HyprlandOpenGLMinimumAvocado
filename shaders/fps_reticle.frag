#version 330 core

in vec2 vScreenPos; // Pixel offset relative to crosshair center

out vec4 FragColor;

// Center Dot
uniform bool  uShowDot;
uniform float uDotRadius;         // e.g. 2.0 pixels
uniform vec4  uDotColor;

// 4-Bar Crosshair
uniform bool  uShowCrosshair;
uniform float uCrosshairLength;    // Length of each tick in pixels (e.g. 10.0)
uniform float uCrosshairThickness; // Thickness of each tick in pixels (e.g. 2.0)
uniform float uCrosshairSpread;    // Distance from center in pixels (e.g. 6.0 + bloom)
uniform vec4  uCrosshairColor;

// Outlines
uniform bool  uShowOutline;
uniform float uOutlineThickness;   // e.g. 1.0 pixel
uniform vec4  uOutlineColor;       // e.g. vec4(0.0, 0.0, 0.0, 0.8)

// Hitmarker (diagonal X ticks)
uniform float uHitmarkerAlpha;     // 0.0 to 1.0 (decays after hit)
uniform float uHitmarkerLength;    // e.g. 8.0 pixels
uniform float uHitmarkerSpread;    // e.g. 5.0 pixels
uniform vec4  uHitmarkerColor;     // e.g. vec4(1.0, 0.2, 0.2, 1.0)

// Helper: signed distance to an axis-aligned box centered at origin
float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

void main() {
    vec2 p = vScreenPos;
    vec4 finalColor = vec4(0.0);

    // 1. Center Dot
    if (uShowDot) {
        float dDot = length(p) - uDotRadius;
        if (uShowOutline) {
            float outlineAlpha = 1.0 - smoothstep(0.0, 1.0, dDot + uOutlineThickness);
            finalColor = mix(finalColor, uOutlineColor, outlineAlpha * uOutlineColor.a);
        }
        float dotAlpha = 1.0 - smoothstep(0.0, 1.0, dDot);
        finalColor = mix(finalColor, uDotColor, dotAlpha * uDotColor.a);
    }

    // 2. Crosshair Bars (Left, Right, Top, Bottom)
    if (uShowCrosshair) {
        vec2 absP = abs(p);
        float halfThick = uCrosshairThickness * 0.5;
        float halfLen = uCrosshairLength * 0.5;
        float centerDist = uCrosshairSpread + halfLen;

        // Distance to horizontal bar (right & left)
        vec2 hOffset = vec2(absP.x - centerDist, absP.y);
        float dHorizontal = sdBox(hOffset, vec2(halfLen, halfThick));

        // Distance to vertical bar (top & bottom)
        vec2 vOffset = vec2(absP.x, absP.y - centerDist);
        float dVertical = sdBox(vOffset, vec2(halfThick, halfLen));

        float dCrosshair = min(dHorizontal, dVertical);

        // Draw crosshair outline
        if (uShowOutline) {
            float crossOutlineAlpha = 1.0 - smoothstep(0.0, 1.0, dCrosshair - uOutlineThickness);
            finalColor = mix(finalColor, uOutlineColor, crossOutlineAlpha * uOutlineColor.a);
        }

        // Draw crosshair fill
        float crossAlpha = 1.0 - smoothstep(0.0, 1.0, dCrosshair);
        finalColor = mix(finalColor, uCrosshairColor, crossAlpha * uCrosshairColor.a);
    }

    // 3. Hitmarker (45-degree diagonal X ticks)
    if (uHitmarkerAlpha > 0.01) {
        // Rotate coordinate 45 degrees
        const float SQRT2_INV = 0.70710678;
        vec2 rotP = vec2(
            (p.x - p.y) * SQRT2_INV,
            (p.x + p.y) * SQRT2_INV
        );
        vec2 absRotP = abs(rotP);

        float hmHalfThick = 1.0;
        float hmHalfLen = uHitmarkerLength * 0.5;
        float hmCenterDist = uHitmarkerSpread + hmHalfLen;

        vec2 hmH = vec2(absRotP.x - hmCenterDist, absRotP.y);
        float dH = sdBox(hmH, vec2(hmHalfLen, hmHalfThick));

        vec2 hmV = vec2(absRotP.x, absRotP.y - hmCenterDist);
        float dV = sdBox(hmV, vec2(hmHalfThick, hmHalfLen));

        float dHitmarker = min(dH, dV);

        // Draw outline
        if (uShowOutline) {
            float hmOutAlpha = 1.0 - smoothstep(0.0, 1.0, dHitmarker - 1.0);
            vec4 hmOutColor = vec4(0.0, 0.0, 0.0, uHitmarkerAlpha * 0.8);
            finalColor = mix(finalColor, hmOutColor, hmOutAlpha);
        }

        float hmAlpha = 1.0 - smoothstep(0.0, 1.0, dHitmarker);
        vec4 hmColor = uHitmarkerColor;
        hmColor.a *= uHitmarkerAlpha;
        finalColor = mix(finalColor, hmColor, hmAlpha * hmColor.a);
    }

    // Discard transparent pixels to avoid filling empty bounding quad
    if (finalColor.a < 0.001) {
        discard;
    }

    FragColor = finalColor;
}
