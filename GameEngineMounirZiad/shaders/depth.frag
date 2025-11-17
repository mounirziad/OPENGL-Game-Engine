#version 330 core
out vec4 FragColor;

void main() {
    // Visualize depth as grayscale
    float depth = gl_FragCoord.z;
    depth = pow(depth, 0.5); // Gamma correction for better visibility
    FragColor = vec4(vec3(depth), 1.0);
}