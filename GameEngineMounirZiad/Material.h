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

    // GI parameters
    glm::vec3 albedo = glm::vec3(1.0f);
    float emissive = 0.0f;
    glm::vec3 emissiveColor = glm::vec3(1.0f);

    // Wireframe parameters
    bool wireframe = false;
    float wireframeThickness = 1.0f;

    // Texture control
    bool useTexture = true;

    // GI control
    bool receiveGI = true;
    bool castShadows = true;

    Material() = default;
    Material(Shader* shader, const glm::vec3& color = glm::vec3(1.0f))
        : shader(shader), color(color), albedo(color) {
    }

    Material(Shader* shader, unsigned int textureID, const glm::vec3& color = glm::vec3(1.0f))
        : shader(shader), textureID(textureID), color(color), albedo(color) {
    }

    void Bind() const {
        if (!shader) return;
        shader->use();

        // Only set texture-related uniforms for shaders that use textures
        bool shouldUseTexture = useTexture && textureID != 0;

        // Check if this shader uses the "useTexture" uniform
        GLint useTextureLoc = glGetUniformLocation(shader->ID, "useTexture");
        if (useTextureLoc != -1) {
            shader->setBool("useTexture", shouldUseTexture);
        }

        if (shouldUseTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureID);

            // Only set texture uniforms if they exist in the shader
            GLint textureLoc = glGetUniformLocation(shader->ID, "texture_diffuse1");
            if (textureLoc != -1) {
                shader->setInt("texture_diffuse1", 0);
            }

            GLint materialColorLoc = glGetUniformLocation(shader->ID, "materialColor");
            if (materialColorLoc != -1) {
                shader->setVec3("materialColor", color);
            }
        }
        else {
            // Only set color uniforms if they exist in the shader
            GLint objectColorLoc = glGetUniformLocation(shader->ID, "objectColor");
            if (objectColorLoc != -1) {
                shader->setVec3("objectColor", color);
            }

            GLint materialColorLoc = glGetUniformLocation(shader->ID, "materialColor");
            if (materialColorLoc != -1) {
                shader->setVec3("materialColor", color);
            }
        }

        // Only set PBR parameters if the shader has these uniforms
        GLint roughnessLoc = glGetUniformLocation(shader->ID, "roughness");
        if (roughnessLoc != -1) {
            shader->setFloat("roughness", roughness);
        }

        GLint metallicLoc = glGetUniformLocation(shader->ID, "metallic");
        if (metallicLoc != -1) {
            shader->setFloat("metallic", metallic);
        }

        // Only set GI parameters if the shader has these uniforms
        GLint albedoLoc = glGetUniformLocation(shader->ID, "albedo");
        if (albedoLoc != -1) {
            shader->setVec3("albedo", albedo);
        }

        GLint emissiveLoc = glGetUniformLocation(shader->ID, "emissive");
        if (emissiveLoc != -1) {
            shader->setFloat("emissive", emissive);
        }

        GLint emissiveColorLoc = glGetUniformLocation(shader->ID, "emissiveColor");
        if (emissiveColorLoc != -1) {
            shader->setVec3("emissiveColor", emissiveColor);
        }

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

    void SetEmissive(float intensity, const glm::vec3& color = glm::vec3(1.0f)) {
        emissive = intensity;
        emissiveColor = color;
    }

    bool HasTexture() const {
        return textureID != 0;
    }

    // Utility function to create PBR material
    static Material CreatePBRMaterial(Shader* shader, const glm::vec3& baseColor,
        float rough = 0.5f, float metal = 0.0f) {
        Material mat(shader, baseColor);
        mat.roughness = rough;
        mat.metallic = metal;
        mat.albedo = baseColor;
        return mat;
    }

    // Utility function to create emissive material
    static Material CreateEmissiveMaterial(Shader* shader, const glm::vec3& color,
        float intensity = 1.0f) {
        Material mat(shader, color);
        mat.emissive = intensity;
        mat.emissiveColor = color;
        return mat;
    }
};