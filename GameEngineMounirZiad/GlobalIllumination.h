#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// Forward declaration
class Shader;

struct EmissiveLight {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;
};

class GlobalIllumination {
public:
    GlobalIllumination();
    ~GlobalIllumination();

    bool Initialize(int voxelResolution = 128);
    void Update(glm::vec3 cameraPos);
    void Cleanup();

    // Main function to apply GI to the scene
    void ApplyGI(Shader* shader, const glm::mat4& view, const glm::mat4& projection);

    // Add emissive lights from the scene
    void AddEmissiveLight(const glm::vec3& position, const glm::vec3& color, float intensity, float radius = 5.0f);
    void ClearEmissiveLights();

    // Settings
    void SetGIEnabled(bool enabled) { m_enabled = enabled; }
    void SetGIIntensity(float intensity) { m_giIntensity = intensity; }

private:
    bool m_enabled = true;
    float m_giIntensity = 1.0f;

    // Camera and scene data
    glm::vec3 m_cameraPos;

    // Emissive lights
    std::vector<EmissiveLight> m_emissiveLights;
    static const int MAX_EMISSIVE_LIGHTS = 16;

    // Framebuffers and textures
    unsigned int m_giFBO;
    unsigned int m_giTexture;

    // Shaders
    Shader* m_giApplyShader;

    void ApplyEmissiveLights(Shader* shader);
};