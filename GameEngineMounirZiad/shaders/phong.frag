#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

// Material properties
uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

// Shadow mapping
uniform bool shadowsEnabled;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;

// GI uniforms
uniform float giIntensity;
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
vec3 CalculateEmissiveGI(vec3 position, vec3 normal, vec3 objectColor) {
    vec3 totalGI = vec3(0.0);
    
    // Base ambient GI
    totalGI += giAmbient * objectColor;
    
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
        totalGI += light.color * light.intensity * attenuation * NdotL * objectColor;
    }
    
    return totalGI * giIntensity;
}

void main() {
    // Ambient with GI
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Calculate shadow
    float shadow = ShadowCalculation(FragPosLightSpace, norm, lightDir);
    
    // Calculate Global Illumination from emissive lights
    vec3 gi = CalculateEmissiveGI(FragPos, norm, objectColor);
    
    // Combine lighting with shadows and GI
    vec3 result = (ambient + gi + (1.0 - shadow) * (diffuse + specular)) * objectColor;
    
    FragColor = vec4(result, 1.0);
}