#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_roughness;
uniform sampler2D texture_metallic;
uniform vec3 materialColor;
uniform bool useTexture;
uniform float roughness;
uniform float metallic;

const float PI = 3.14159265359;

// Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Geometry Function (Schlick GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Equation (Fresnel-Schlick)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Sample textures
    vec4 albedo;
    float rough;
    float metal;
    
    if (useTexture) {
        albedo = texture(texture_diffuse1, TexCoords);
        // If you have roughness/metallic maps, sample them here:
        // rough = texture(texture_roughness, TexCoords).r;
        // metal = texture(texture_metallic, TexCoords).r;
        rough = roughness;
        metal = metallic;
    } else {
        albedo = vec4(materialColor, 1.0);
        rough = roughness;
        metal = metallic;
    }

    if (albedo.a < 0.1) discard;

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightPos - FragPos);
    vec3 H = normalize(V + L);

    // Calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metal);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, rough);
    float G = GeometrySmith(N, V, L, rough);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;
    // For energy conservation, the diffuse and specular light can't be above 1.0
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metal;

    // Scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // Add to outgoing radiance Lo
    vec3 radiance = lightColor;
    vec3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL;

    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo.rgb;

    vec3 color = ambient + Lo;
    
    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // Gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, albedo.a);
}