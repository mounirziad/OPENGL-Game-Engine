#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform float bloomThreshold = 1.0;

void main() {
    vec3 color = texture(scene, TexCoords).rgb;
    
    // Extract bright areas for bloom
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > bloomThreshold) {
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}