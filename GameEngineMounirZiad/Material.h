#pragma once
#include "Shader.h"
#include <glm/glm.hpp>
#include <string>

struct Material {
    Shader* shader = nullptr;
    glm::vec3 color = glm::vec3(1.0f);
    unsigned int textureID = 0;
    std::string texturePath;

    // PBR parameters
    float roughness = 0.5f;
    float metallic = 0.0f;

    // Wireframe parameters
    bool wireframe = false;
    float wireframeThickness = 1.0f;

    // Texture control
    bool useTexture = true;

    Material() = default;
    Material(Shader* shader, const glm::vec3& color = glm::vec3(1.0f))
        : shader(shader), color(color) {
    }

    Material(Shader* shader, unsigned int textureID, const glm::vec3& color = glm::vec3(1.0f))
        : shader(shader), textureID(textureID), color(color) {
    }

    void Bind() const {
        if (!shader) return;
        shader->use();

        // Set texture usage flag - only use texture if available and enabled
        bool shouldUseTexture = useTexture && textureID != 0;
        shader->setBool("useTexture", shouldUseTexture);

        if (shouldUseTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureID);
            shader->setInt("texture_diffuse1", 0);
            // Also set material color for shaders that might mix texture with color
            shader->setVec3("materialColor", color);
        }
        else {
            // Use solid color when texture is disabled or not available
            shader->setVec3("objectColor", color);
            shader->setVec3("materialColor", color);
        }

        // Set PBR parameters if using PBR shader
        shader->setFloat("roughness", roughness);
        shader->setFloat("metallic", metallic);

        // Handle wireframe mode
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(wireframeThickness);
        }
    }

    void Unbind() const {
        // Reset polygon mode if wireframe was enabled
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }

    void SetWireframe(bool enabled) {
        wireframe = enabled;
    }

    void SetUseTexture(bool enabled) {
        useTexture = enabled;
    }

    bool HasTexture() const {
        return textureID != 0;
    }
};