#version 330 core
out vec4 FragColor;
out vec4 BrightColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

// Material properties
uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float emissive;
uniform vec3 emissiveColor;

// Lighting
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// Shadow mapping
uniform bool shadowsEnabled;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

// GI uniforms
uniform float giIntensity;
uniform vec3 cameraPos;
uniform vec3 giAmbient;

// Emissive lights
uniform int emissiveLightCount;
struct EmissiveLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};
uniform EmissiveLight emissiveLights[16];

// Shadow calculation function
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (!shadowsEnabled) return 0.0;
    
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if fragment is outside light frustum
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;
    
    // Get closest depth value from light's perspective
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    
    // Check if current fragment is in shadow with bias
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    
    return shadow;
}

// Calculate contribution from emissive lights
vec3 CalculateEmissiveGI(vec3 position, vec3 normal, vec3 albedo) {
    vec3 totalGI = vec3(0.0);
    
    // Base ambient GI
    totalGI += giAmbient * albedo;
    
    // Add contribution from each emissive light
    for (int i = 0; i < emissiveLightCount; i++) {
        EmissiveLight light = emissiveLights[i];
        
        // Calculate distance to light
        vec3 toLight = light.position - position;
        float distance = length(toLight);
        
        // Skip if too far
        if (distance > light.radius) continue;
        
        // Normalize direction
        vec3 lightDir = normalize(toLight);
        
        // Calculate attenuation (inverse square falloff)
        float attenuation = 1.0 / (1.0 + distance * distance);
        
        // Calculate light contribution based on angle
        float NdotL = max(dot(normal, lightDir), 0.0);
        
        // Add to total GI
        totalGI += light.color * light.intensity * attenuation * NdotL * albedo;
    }
    
    return totalGI * giIntensity;
}

void main() {
    // Normal lighting calculations
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = normalize(lightPos - FragPos);
    vec3 H = normalize(V + L);
    
    // Simple PBR approximation
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    
    // Cook-Torrance BRDF
    float alpha = roughness * roughness;
    float D = (alpha * alpha) / (3.14159 * pow((NdotH * NdotH) * (alpha * alpha - 1.0) + 1.0, 2.0));
    
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1 = NdotV / (NdotV * (1.0 - k) + k);
    float G2 = NdotL / (NdotL * (1.0 - k) + k);
    float G = G1 * G2;
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    
    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159;
    
    vec3 radiance = lightColor * NdotL;
    
    // Calculate shadow
    float shadow = ShadowCalculation(FragPosLightSpace, N, L);
    
    // Apply shadow to direct lighting
    vec3 Lo = (diffuse + specular) * radiance * (1.0 - shadow);
    
    // Calculate Global Illumination from emissive lights
    vec3 gi = CalculateEmissiveGI(FragPos, N, albedo);
    
    // Emissive contribution (this object's own emission)
    vec3 emissiveContribution = emissive * emissiveColor;
    
    // Final color with GI and emissive
    vec3 color = gi + Lo + emissiveContribution;
    
    // Output to main scene buffer
    FragColor = vec4(color, 1.0);
    
    // Output to emissive buffer - ONLY if material has emissive property
    if (emissive > 0.1) {
        BrightColor = vec4(emissiveContribution, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}