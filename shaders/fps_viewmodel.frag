#version 330 core

in vec3 vWorldPos;
in vec2 vTexCoords;
in vec3 vNormal;
in mat3 vTBN;
in vec3 vViewDir;

out vec4 FragColor;

const float PI = 3.14159265359;
const int MAX_POINT_LIGHTS = 4;

// Material textures
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicRoughnessMap;
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;

// Material factors & flags
uniform bool  uUseAlbedoMap;
uniform bool  uUseNormalMap;
uniform bool  uUseMetallicRoughnessMap;
uniform bool  uUseAOMap;
uniform bool  uUseEmissiveMap;

uniform vec4  uBaseColorFactor;       // Default: vec4(1.0)
uniform float uMetallicFactor;        // Default: 0.0
uniform float uRoughnessFactor;       // Default: 0.5
uniform float uAOFactor;              // Default: 1.0
uniform vec3  uEmissiveFactor;        // Default: vec3(0.0)

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

// Viewmodel-specific lighting additions
// 1. Muzzle Flash dynamic impulse light
uniform vec3  uMuzzleFlashPos;
uniform vec3  uMuzzleFlashColor;
uniform float uMuzzleFlashIntensity; // 0.0 when not firing

// 2. Viewmodel Rim lighting (silhouette highlight against dark game geometry)
uniform vec3  uRimColor;             // Default: vec3(0.2)
uniform float uRimPower;             // Default: 3.0
uniform float uRimIntensity;         // Default: 0.5

// 3. Viewmodel readability ambient boost
uniform vec3  uAmbientColor;
uniform float uViewmodelAmbientBoost; // Extra fill light for viewmodel clarity

uniform float uExposure;

// -----------------------------------------------------------------------------
// PBR Helpers
// -----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 EvaluateDirectLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
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
    // Material parameters
    vec4 baseColor = uBaseColorFactor;
    if (uUseAlbedoMap) {
        vec4 tex = texture(uAlbedoMap, vTexCoords);
        tex.rgb = pow(tex.rgb, vec3(2.2));
        baseColor *= tex;
    }
    vec3 albedo = baseColor.rgb;
    float alpha = baseColor.a;

    // Normal
    vec3 N = normalize(vNormal);
    if (uUseNormalMap) {
        vec3 tNorm = texture(uNormalMap, vTexCoords).xyz * 2.0 - 1.0;
        N = normalize(vTBN * tNorm);
    }

    // Metallic-Roughness
    float metallic = clamp(uMetallicFactor, 0.0, 1.0);
    float roughness = clamp(uRoughnessFactor, 0.04, 1.0);
    if (uUseMetallicRoughnessMap) {
        vec3 mr = texture(uMetallicRoughnessMap, vTexCoords).rgb;
        roughness *= clamp(mr.g, 0.04, 1.0);
        metallic *= clamp(mr.b, 0.0, 1.0);
    }

    // AO
    float ao = clamp(uAOFactor, 0.0, 1.0);
    if (uUseAOMap) {
        ao *= texture(uAOMap, vTexCoords).r;
    }

    // Emissive
    vec3 emissive = uEmissiveFactor;
    if (uUseEmissiveMap) {
        vec3 em = texture(uEmissiveMap, vTexCoords).rgb;
        emissive *= pow(em, vec3(2.2));
    }

    vec3 V = normalize(vViewDir);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // Directional light
    if (uEnableDirLight) {
        vec3 L = normalize(-uDirLight.direction);
        vec3 radiance = uDirLight.color * uDirLight.intensity;
        Lo += EvaluateDirectLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // Point lights
    int count = min(uNumPointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < count; ++i) {
        vec3 lightDelta = uPointLights[i].position - vWorldPos;
        float dist = length(lightDelta);
        if (dist > uPointLights[i].radius && uPointLights[i].radius > 0.0) continue;

        vec3 L = normalize(lightDelta);
        float atten = 1.0 / max(dist * dist, 0.0001);
        if (uPointLights[i].radius > 0.0) {
            float cutoff = clamp(1.0 - pow(dist / uPointLights[i].radius, 4.0), 0.0, 1.0);
            atten *= cutoff * cutoff;
        }
        vec3 radiance = uPointLights[i].color * (uPointLights[i].intensity * atten);
        Lo += EvaluateDirectLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // Dynamic Muzzle Flash light
    if (uMuzzleFlashIntensity > 0.001) {
        vec3 mfDelta = uMuzzleFlashPos - vWorldPos;
        float mfDist = length(mfDelta);
        vec3 Lmf = normalize(mfDelta);
        float mfAtten = 1.0 / max(mfDist * mfDist, 0.01);
        vec3 mfRadiance = uMuzzleFlashColor * (uMuzzleFlashIntensity * mfAtten);
        Lo += EvaluateDirectLight(Lmf, mfRadiance, N, V, albedo, metallic, roughness, F0);
    }

    // Viewmodel Ambient with readability boost
    float ambientFill = 1.0 + max(uViewmodelAmbientBoost, 0.0);
    vec3 ambient = uAmbientColor * albedo * ao * ambientFill;

    // Viewmodel Rim lighting (grazing angles)
    float rimFactor = 1.0 - max(dot(N, V), 0.0);
    float rimPower = (uRimPower > 0.0) ? uRimPower : 3.0;
    vec3 rimLight = uRimColor * (pow(rimFactor, rimPower) * uRimIntensity);

    vec3 finalColor = ambient + Lo + emissive + rimLight;

    // HDR Exposure Tonemap & Gamma Correction
    float exposure = (uExposure > 0.0) ? uExposure : 1.0;
    finalColor = vec3(1.0) - exp(-finalColor * exposure);
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    FragColor = vec4(finalColor, alpha);
}
