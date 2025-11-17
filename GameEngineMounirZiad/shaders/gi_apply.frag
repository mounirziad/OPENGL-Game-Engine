#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

// Material properties
uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float giIntensity;

// Light properties  
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// GI properties
uniform bool giEnabled;

void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // Basic direct lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Basic specular
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * lightColor;
    
    // Direct lighting result
    vec3 directLighting = (diffuse + specular * metallic) * albedo;
    
    // Global Illumination component
    vec3 giResult = vec3(0.0);
    if (giEnabled) {
        // Ambient occlusion approximation
        float ambientOcclusion = 0.5 + 0.5 * dot(norm, vec3(0.0, 1.0, 0.0));
        
        // Color bleeding simulation
        vec3 indirectDiffuse = albedo * 0.3 * ambientOcclusion;
        
        // Roughness-based specular reflection
        vec3 R = reflect(-viewDir, norm);
        float giSpecular = pow(max(dot(R, vec3(0.0, 1.0, 0.0)), 0.0), 16.0) * (1.0 - roughness);
        
        // Combine GI components
        giResult = (indirectDiffuse + vec3(0.1) * giSpecular * metallic) * giIntensity;
    }
    
    // Final combination
    vec3 finalColor = directLighting + giResult * albedo;
    
    FragColor = vec4(finalColor, 1.0);
}