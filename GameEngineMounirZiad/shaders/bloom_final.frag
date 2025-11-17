#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float bloomIntensity = 0.5;

void main() {
    vec3 sceneColor = texture(scene, TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    
    // Combine scene with bloom
    vec3 result = sceneColor + bloomColor * bloomIntensity;
    
    FragColor = vec4(result, 1.0);
}