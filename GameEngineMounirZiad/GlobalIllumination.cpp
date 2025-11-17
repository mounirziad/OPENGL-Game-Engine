#include "GlobalIllumination.h"
#include "ShaderManager.h"
#include "Registry.h"
#include "Component.h"
#include <iostream>

GlobalIllumination::GlobalIllumination()
    : m_giIntensity(1.0f), m_enabled(true),
    m_giFBO(0), m_giTexture(0), m_giApplyShader(nullptr) {
}

GlobalIllumination::~GlobalIllumination() {
    Cleanup();
}

bool GlobalIllumination::Initialize(int voxelResolution) {
    // Create a simple GI shader for light propagation
    m_giApplyShader = SHADER_MANAGER.GetShader(ShaderManager::GI_APPLY);
    if (!m_giApplyShader) {
        std::cout << "Failed to load GI apply shader!" << std::endl;
        return false;
    }

    // Create framebuffer for GI calculations (optional for now)
    glGenFramebuffers(1, &m_giFBO);
    glGenTextures(1, &m_giTexture);

    std::cout << "Global Illumination system initialized" << std::endl;
    return true;
}

void GlobalIllumination::Update(glm::vec3 cameraPos) {
    if (!m_enabled) return;

    // Store camera position for GI calculations
    m_cameraPos = cameraPos;

    // Clear previous frame's lights
    ClearEmissiveLights();
}

void GlobalIllumination::AddEmissiveLight(const glm::vec3& position, const glm::vec3& color, float intensity, float radius) {
    if (m_emissiveLights.size() < MAX_EMISSIVE_LIGHTS) {
        EmissiveLight light;
        light.position = position;
        light.color = color;
        light.intensity = intensity;
        light.radius = radius;
        m_emissiveLights.push_back(light);
    }
}

void GlobalIllumination::ClearEmissiveLights() {
    m_emissiveLights.clear();
}

void GlobalIllumination::ApplyGI(Shader* shader, const glm::mat4& view, const glm::mat4& projection) {
    if (!m_enabled || !shader) return;

    shader->use();

    // Set GI parameters - these will be used in your fragment shader
    shader->setFloat("giIntensity", m_giIntensity);
    shader->setVec3("cameraPos", m_cameraPos);

    // Pass emissive lights to the shader
    ApplyEmissiveLights(shader);
}

void GlobalIllumination::ApplyEmissiveLights(Shader* shader) {
    // Set base ambient GI
    shader->setVec3("giAmbient", glm::vec3(0.1f, 0.1f, 0.1f) * m_giIntensity);

    // Pass emissive lights to shader
    int lightCount = std::min((int)m_emissiveLights.size(), MAX_EMISSIVE_LIGHTS);
    shader->setInt("emissiveLightCount", lightCount);

    for (int i = 0; i < lightCount; i++) {
        std::string base = "emissiveLights[" + std::to_string(i) + "].";
        shader->setVec3(base + "position", m_emissiveLights[i].position);
        shader->setVec3(base + "color", m_emissiveLights[i].color);
        shader->setFloat(base + "intensity", m_emissiveLights[i].intensity);
        shader->setFloat(base + "radius", m_emissiveLights[i].radius);
    }
}

void GlobalIllumination::Cleanup() {
    if (m_giFBO) {
        glDeleteFramebuffers(1, &m_giFBO);
        m_giFBO = 0;
    }
    if (m_giTexture) {
        glDeleteTextures(1, &m_giTexture);
        m_giTexture = 0;
    }
}