#version 330 core

// -----------------------------------------------------------------------------
// OBJ Mesh Fragment Shader
// Compatible with standard OBJ .mtl properties and modern PBR texture sets
// (Albedo, Normal, Metallic-Roughness, Ambient Occlusion, and Emissive)
// -----------------------------------------------------------------------------

in vec3 vWorldPos;
in vec2 vTexCoords;
in vec3 vNormal;
in mat3 vTBN;

out vec4 FragColor;

const float PI = 3.14159265359;
const int MAX_POINT_LIGHTS = 8;

// PBR Texture Maps (matching the standard layout)
uniform sampler2D uAlbedoMap;            // Texture Unit 0
uniform sampler2D uNormalMap;            // Texture Unit 1
uniform sampler2D uMetallicRoughnessMap; // Texture Unit 2 (G: Roughness, B: Metallic)
uniform sampler2D uAOMap;                // Texture Unit 3
uniform sampler2D uEmissiveMap;          // Texture Unit 4

// Texture usage flags
uniform bool uUseAlbedoMap;
uniform bool uUseNormalMap;
uniform bool uUseMetallicRoughnessMap;
uniform bool uUseAOMap;
uniform bool uUseEmissiveMap;

// Material uniform factors (defaults or OBJ .mtl fallbacks)
uniform vec4  uBaseColorFactor; // OBJ Kd / diffuse tint (default: vec4(1.0))
uniform float uMetallicFactor;  // OBJ Pm / metallic (default: 0.0)
uniform float uRoughnessFactor; // OBJ Pr / roughness (default: 0.5)
uniform float uAOFactor;        // Ambient occlusion multiplier (default: 1.0)
uniform vec3  uEmissiveFactor;  // OBJ Ke / emissive factor (default: vec3(0.0))
uniform float uAlphaCutoff;     // Alpha cutoff for masked materials (e.g. foliage)

// Camera
uniform vec3 uCameraPos;

// Directional Light
struct DirectionalLight {
    vec3  direction;
    vec3  color;
    float intensity;
};
uniform bool             uEnableDirLight;
uniform DirectionalLight uDirLight;

// Point Lights
struct PointLight {
    vec3  position;
    vec3  color;
    float intensity;
    float radius;
};
uniform int        uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

// Environment & Exposure
uniform vec3  uAmbientColor;
uniform float uExposure;

// Debug visualization modes for inspecting OBJ models & textures:
// 0: Full PBR Lit
// 1: Albedo Map
// 2: World Normals
// 3: Roughness
// 4: Metallic
// 5: Ambient Occlusion
// 6: Lighting only (white clay)
uniform int uDebugMode;

// -----------------------------------------------------------------------------
// PBR Math
// -----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 EvaluateLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // 1. Albedo & Alpha
    vec4 baseColor = uBaseColorFactor;
    if (uUseAlbedoMap) {
        vec4 texColor = texture(uAlbedoMap, vTexCoords);
        texColor.rgb = pow(texColor.rgb, vec3(2.2)); // sRGB to linear
        baseColor *= texColor;
    }

    if (uAlphaCutoff > 0.0 && baseColor.a < uAlphaCutoff) {
        discard;
    }

    vec3 albedo = baseColor.rgb;
    float alpha = baseColor.a;

    // 2. Normal Mapping
    vec3 N = normalize(vNormal);
    if (uUseNormalMap) {
        vec3 tNorm = texture(uNormalMap, vTexCoords).xyz * 2.0 - 1.0;
        N = normalize(vTBN * tNorm);
    }

    // 3. Metallic & Roughness
    float metallic = clamp(uMetallicFactor, 0.0, 1.0);
    float roughness = clamp(uRoughnessFactor, 0.04, 1.0);
    if (uUseMetallicRoughnessMap) {
        vec3 mrSample = texture(uMetallicRoughnessMap, vTexCoords).rgb;
        roughness *= clamp(mrSample.g, 0.04, 1.0);
        metallic *= clamp(mrSample.b, 0.0, 1.0);
    }

    // 4. Ambient Occlusion
    float ao = clamp(uAOFactor, 0.0, 1.0);
    if (uUseAOMap) {
        ao *= texture(uAOMap, vTexCoords).r;
    }

    // 5. Emissive
    vec3 emissive = uEmissiveFactor;
    if (uUseEmissiveMap) {
        vec3 em = texture(uEmissiveMap, vTexCoords).rgb;
        emissive *= pow(em, vec3(2.2));
    }

    // -------------------------------------------------------------------------
    // Debug Visualizer Passes
    // -------------------------------------------------------------------------
    if (uDebugMode == 1) { FragColor = vec4(albedo, 1.0); return; }
    if (uDebugMode == 2) { FragColor = vec4(N * 0.5 + 0.5, 1.0); return; }
    if (uDebugMode == 3) { FragColor = vec4(vec3(roughness), 1.0); return; }
    if (uDebugMode == 4) { FragColor = vec4(vec3(metallic), 1.0); return; }
    if (uDebugMode == 5) { FragColor = vec4(vec3(ao), 1.0); return; }

    // 6. Direct Lighting Calculations
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 diffuseColor = (uDebugMode == 6) ? vec3(0.8) : albedo; // White clay mode
    vec3 F0 = mix(vec3(0.04), diffuseColor, metallic);
    vec3 Lo = vec3(0.0);

    // Directional Light
    if (uEnableDirLight) {
        vec3 L = normalize(-uDirLight.direction);
        vec3 radiance = uDirLight.color * uDirLight.intensity;
        Lo += EvaluateLight(L, radiance, N, V, diffuseColor, metallic, roughness, F0);
    }

    // Point Lights
    int numLights = min(uNumPointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < numLights; ++i) {
        vec3 lightDelta = uPointLights[i].position - vWorldPos;
        float distance = length(lightDelta);
        if (distance > uPointLights[i].radius && uPointLights[i].radius > 0.0) continue;

        vec3 L = normalize(lightDelta);
        float atten = 1.0 / max(distance * distance, 0.0001);
        if (uPointLights[i].radius > 0.0) {
            float cutoff = clamp(1.0 - pow(distance / uPointLights[i].radius, 4.0), 0.0, 1.0);
            atten *= cutoff * cutoff;
        }

        vec3 radiance = uPointLights[i].color * (uPointLights[i].intensity * atten);
        Lo += EvaluateLight(L, radiance, N, V, diffuseColor, metallic, roughness, F0);
    }

    // Ambient Lighting
    vec3 ambient = uAmbientColor * diffuseColor * ao;

    // Total radiance
    vec3 color = ambient + Lo + emissive;

    // HDR Exposure Tone Mapping
    float exposure = (uExposure > 0.0) ? uExposure : 1.0;
    color = vec3(1.0) - exp(-color * exposure);

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, alpha);
}
