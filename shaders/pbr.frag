#version 330 core

in vec3 vWorldPos;
in vec2 vTexCoords;
in vec3 vNormal;
in mat3 vTBN;

out vec4 FragColor;

const float PI = 3.14159265359;
const int MAX_POINT_LIGHTS = 8;

// Material maps
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicRoughnessMap; // Green = Roughness, Blue = Metallic (glTF standard)
uniform sampler2D uAOMap;
uniform sampler2D uEmissiveMap;

// Material fallback / factor uniforms
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

// Camera
uniform vec3  uCameraPos;

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

// Ambient & Environment
uniform vec3  uAmbientColor; // Background / sky ambient radiance
uniform float uExposure;     // HDR exposure tone mapping

// -----------------------------------------------------------------------------
// PBR Math Functions
// -----------------------------------------------------------------------------

// Trowbridge-Reitz GGX Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

// Smith's Schlick-GGX Geometric Shadowing / Masking
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Evaluate Cook-Torrance Specular + Lambertian Diffuse for one light source
vec3 EvaluateDirectLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic); // Metals have no diffuse reflection

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // Albedo & Alpha
    vec4 baseColor = uBaseColorFactor;
    if (uUseAlbedoMap) {
        vec4 texColor = texture(uAlbedoMap, vTexCoords);
        // Convert sRGB texture to linear color space
        texColor.rgb = pow(texColor.rgb, vec3(2.2));
        baseColor *= texColor;
    }
    vec3 albedo = baseColor.rgb;
    float alpha = baseColor.a;

    // Normal mapping
    vec3 N = normalize(vNormal);
    if (uUseNormalMap) {
        vec3 tangentNormal = texture(uNormalMap, vTexCoords).xyz * 2.0 - 1.0;
        N = normalize(vTBN * tangentNormal);
    }

    // Metallic and Roughness
    float metallic = clamp(uMetallicFactor, 0.0, 1.0);
    float roughness = clamp(uRoughnessFactor, 0.04, 1.0);
    if (uUseMetallicRoughnessMap) {
        vec3 mrSample = texture(uMetallicRoughnessMap, vTexCoords).rgb;
        roughness *= clamp(mrSample.g, 0.04, 1.0);
        metallic *= clamp(mrSample.b, 0.0, 1.0);
    }

    // Ambient Occlusion
    float ao = clamp(uAOFactor, 0.0, 1.0);
    if (uUseAOMap) {
        ao *= texture(uAOMap, vTexCoords).r;
    }

    // Emissive
    vec3 emissive = uEmissiveFactor;
    if (uUseEmissiveMap) {
        vec3 emissiveSample = texture(uEmissiveMap, vTexCoords).rgb;
        emissiveSample = pow(emissiveSample, vec3(2.2));
        emissive *= emissiveSample;
    }

    // View direction
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Base reflectivity (F0): 0.04 for dielectrics, tinted albedo for metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Direct lighting accumulation (Lo)
    vec3 Lo = vec3(0.0);

    // 1. Directional Light
    if (uEnableDirLight) {
        vec3 L = normalize(-uDirLight.direction);
        vec3 radiance = uDirLight.color * uDirLight.intensity;
        Lo += EvaluateDirectLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // 2. Point Lights
    int numLights = min(uNumPointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < numLights; ++i) {
        vec3 lightToPos = uPointLights[i].position - vWorldPos;
        float distance = length(lightToPos);

        if (distance > uPointLights[i].radius && uPointLights[i].radius > 0.0) {
            continue;
        }

        vec3 L = normalize(lightToPos);

        // Smooth windowed distance attenuation
        float atten = 1.0 / max(distance * distance, 0.0001);
        if (uPointLights[i].radius > 0.0) {
            float cutoff = clamp(1.0 - pow(distance / uPointLights[i].radius, 4.0), 0.0, 1.0);
            atten *= cutoff * cutoff;
        }

        vec3 radiance = uPointLights[i].color * (uPointLights[i].intensity * atten);
        Lo += EvaluateDirectLight(L, radiance, N, V, albedo, metallic, roughness, F0);
    }

    // Ambient Lighting (diffuse irradiance approximation modulated by AO)
    vec3 ambient = uAmbientColor * albedo * ao;

    // Total radiance
    vec3 color = ambient + Lo + emissive;

    // HDR Exposure Tone Mapping
    float exposure = (uExposure > 0.0) ? uExposure : 1.0;
    color = vec3(1.0) - exp(-color * exposure);

    // Gamma correction (linear -> sRGB)
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, alpha);
}
