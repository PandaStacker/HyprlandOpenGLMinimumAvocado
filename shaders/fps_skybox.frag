#version 330 core

in vec3 vTexCoords;

out vec4 FragColor;

// Cubemap mode
uniform samplerCube uSkybox;
uniform bool        uUseCubemap;

// Procedural atmosphere fallback mode
uniform vec3  uSunDir;          // Normalized direction to the sun
uniform vec3  uSunColor;        // Sun disk color (e.g. vec3(1.0, 0.95, 0.8))
uniform vec3  uSkyZenithColor;  // Top of sky (e.g. vec3(0.15, 0.35, 0.75))
uniform vec3  uSkyHorizonColor; // Horizon (e.g. vec3(0.65, 0.75, 0.85))
uniform vec3  uGroundColor;     // Ground (e.g. vec3(0.2, 0.18, 0.15))
uniform float uSunSize;         // Angular size of sun disk (e.g. 0.998)
uniform float uSunBloom;        // Glare factor (e.g. 16.0)

void main() {
    vec3 dir = normalize(vTexCoords);

    if (uUseCubemap) {
        FragColor = texture(uSkybox, dir);
        return;
    }

    // Procedural atmospheric sky model
    float y = dir.y;

    // Atmospheric gradient
    vec3 skyColor;
    if (y > 0.0) {
        // Sky hemisphere (horizon -> zenith)
        float factor = pow(y, 0.6);
        skyColor = mix(uSkyHorizonColor, uSkyZenithColor, factor);
    } else {
        // Ground hemisphere (horizon -> ground)
        float factor = pow(clamp(-y, 0.0, 1.0), 0.5);
        skyColor = mix(uSkyHorizonColor, uGroundColor, factor);
    }

    // Sun disk and corona bloom
    vec3 sunDirection = normalize(uSunDir);
    float sunDot = dot(dir, sunDirection);

    if (sunDot > 0.0) {
        // Sun corona glare
        float bloom = pow(sunDot, uSunBloom) * 0.4;
        skyColor += uSunColor * bloom;

        // Sharp sun disk
        float sunDiskThreshold = (uSunSize > 0.0) ? uSunSize : 0.998;
        if (sunDot > sunDiskThreshold) {
            float sunDiskEdge = smoothstep(sunDiskThreshold, sunDiskThreshold + 0.001, sunDot);
            skyColor = mix(skyColor, uSunColor * 3.0, sunDiskEdge);
        }
    }

    FragColor = vec4(skyColor, 1.0);
}
