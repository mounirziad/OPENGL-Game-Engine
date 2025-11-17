#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 albedo;
uniform vec3 viewPos;  // Add required uniforms
uniform vec3 lightPos;
uniform vec3 lightColor;

void main() {
    // Simple voxelization visualization with basic lighting
    vec3 color = albedo;
    
    // Add basic directional lighting so it's visible
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.2);  // At least 0.2 for visibility
    vec3 diffuse = diff * lightColor;
    
    // Add grid pattern to distinguish from regular rendering
    vec3 grid = fract(FragPos * 2.0);  // Use world position for grid
    if (grid.x < 0.1 || grid.y < 0.1 || grid.z < 0.1) {
        color *= 0.5; // Darken grid lines more
    }
    
    // Combine with lighting
    vec3 finalColor = color * diffuse;
    
    FragColor = vec4(finalColor, 1.0);
}