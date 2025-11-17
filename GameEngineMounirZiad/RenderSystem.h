#pragma once
#include "Registry.h"
#include "Component.h"
#include "Material.h"
#include "ShaderManager.h"
#include "GlobalIllumination.h"
#include "BloomSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

class RenderSystem {
private:
    GlobalIllumination m_giSystem;
    bool m_giEnabled = true;
    bool m_shadowsEnabled = true;
    BloomSystem m_bloomSystem;
    bool m_bloomEnabled = true;

    // Shadow mapping
    unsigned int m_shadowMapFBO;
    unsigned int m_shadowMapTexture;
    const unsigned int m_shadowWidth = 2048;
    const unsigned int m_shadowHeight = 2048;

    // Light space matrix (consistent between passes)
    glm::mat4 m_lightSpaceMatrix;

public:
    RenderSystem() : m_shadowMapFBO(0), m_shadowMapTexture(0) {}

    bool Initialize(int screenWidth, int screenHeight) {
        // Initialize GI system
        if (!m_giSystem.Initialize(64)) {
            std::cout << "Warning: GI system failed to initialize, continuing without GI" << std::endl;
            m_giEnabled = false;
        }

        // Initialize shadow mapping
        if (!InitializeShadowMapping()) {
            std::cout << "Warning: Shadow mapping failed to initialize" << std::endl;
            m_shadowsEnabled = false;
        }

        // Initialize bloom system
        if (!m_bloomSystem.Initialize(screenWidth, screenHeight)) {
            std::cout << "Warning: Bloom system failed to initialize" << std::endl;
            m_bloomEnabled = false;
        }

        return true;
    }

    void Cleanup() {
        m_giSystem.Cleanup();
        m_bloomSystem.Cleanup();
        CleanupShadowMapping();
    }

    void Render(Registry& registry, Camera& camera, int width, int height,
        const glm::vec3& lightPos, const glm::vec3& lightColor) {

        // Update light space matrix for this frame
        UpdateLightSpaceMatrix(lightPos);

        // Update GI system
        if (m_giEnabled) {
            m_giSystem.Update(camera.Position);
        }

        // First pass: Shadow mapping
        if (m_shadowsEnabled) {
            RenderShadowPass(registry, lightPos);
        }

        // Second pass: Scene capture to HDR buffer for bloom
        m_bloomSystem.BeginSceneCapture();
        RenderMainPass(registry, camera, width, height, lightPos, lightColor);
        m_bloomSystem.EndSceneCapture();

        // Third pass: Apply bloom effects
        m_bloomSystem.ApplyBloom(width, height);
    }

    void SetGIEnabled(bool enabled) {
        m_giEnabled = enabled;
        m_giSystem.SetGIEnabled(enabled);
    }

    void SetShadowsEnabled(bool enabled) {
        m_shadowsEnabled = enabled;
    }

    void SetBloomEnabled(bool enabled) {
        m_bloomEnabled = enabled;
        m_bloomSystem.SetBloomEnabled(enabled);
    }

    void SetBloomIntensity(float intensity) {
        m_bloomSystem.SetBloomIntensity(intensity);
    }

    void SetBloomThreshold(float threshold) {
        m_bloomSystem.SetBloomThreshold(threshold);
    }

    void SetBlurStrength(float strength) {
        m_bloomSystem.SetBlurStrength(strength);
    }

    void SetBlurIterations(int iterations) {
        m_bloomSystem.SetBlurIterations(iterations);
    }

    bool IsGIEnabled() const { return m_giEnabled; }
    bool IsShadowsEnabled() const { return m_shadowsEnabled; }
    bool IsBloomEnabled() const { return m_bloomEnabled; }

private:
    void UpdateLightSpaceMatrix(const glm::vec3& lightPos) {
        // Calculate consistent light space matrix for this frame
        float near_plane = 1.0f, far_plane = 25.0f;
        glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(lightPos,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        m_lightSpaceMatrix = lightProjection * lightView;
    }

    bool InitializeShadowMapping() {
        // Create FBO for shadow mapping
        glGenFramebuffers(1, &m_shadowMapFBO);

        // Create depth texture
        glGenTextures(1, &m_shadowMapTexture);
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
            m_shadowWidth, m_shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // Attach depth texture as FBO's depth buffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMapTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Shadow framebuffer not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void CleanupShadowMapping() {
        if (m_shadowMapFBO) {
            glDeleteFramebuffers(1, &m_shadowMapFBO);
            m_shadowMapFBO = 0;
        }
        if (m_shadowMapTexture) {
            glDeleteTextures(1, &m_shadowMapTexture);
            m_shadowMapTexture = 0;
        }
    }

    void RenderShadowPass(Registry& registry, const glm::vec3& lightPos) {
        glViewport(0, 0, m_shadowWidth, m_shadowHeight);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Use simple depth shader
        Shader* depthShader = SHADER_MANAGER.GetShader(ShaderManager::DEPTH);
        if (!depthShader) {
            std::cout << "No depth shader available, skipping shadow pass" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        depthShader->use();
        depthShader->setMat4("lightSpaceMatrix", m_lightSpaceMatrix);

        // Render all shadow-casting entities
        auto entities = registry.GetEntitiesWith<TransformComponent, MeshComponent>();
        for (auto e : entities) {
            auto transform = registry.GetComponent<TransformComponent>(e);
            auto mesh = registry.GetComponent<MeshComponent>(e);

            if (!transform || !mesh) continue;

            // Skip if material explicitly doesn't cast shadows
            if (mesh->material && !mesh->material->castShadows) continue;

            glm::mat4 model = transform->GetModelMatrix();
            depthShader->setMat4("model", model);

            glBindVertexArray(mesh->VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
            glBindVertexArray(0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Debug: Check shadow map contents (uncomment to debug)
        // CheckShadowMap();
    }

    void RenderMainPass(Registry& registry, Camera& camera, int width, int height,
        const glm::vec3& lightPos, const glm::vec3& lightColor) {

        

        // Make sure we're rendering to both buffers
        unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Clear previous emissive lights
        m_giSystem.ClearEmissiveLights();

        auto entities = registry.GetEntitiesWith<TransformComponent, MeshComponent>();

        for (auto e : entities) {
            auto transform = registry.GetComponent<TransformComponent>(e);
            auto mesh = registry.GetComponent<MeshComponent>(e);

            if (!transform || !mesh) continue;

            // If material is emissive, register it as a light source
            if (mesh->material->emissive > 0.1f) {
                m_giSystem.AddEmissiveLight(
                    transform->position,
                    mesh->material->emissiveColor,
                    mesh->material->emissive,
                    8.0f // radius
                );
            }

            // Use material shader if available, otherwise default to mesh shader
            Shader* shaderToUse = mesh->material ? mesh->material->shader : mesh->shader;
            if (!shaderToUse) continue;

            shaderToUse->use();

            glm::vec3 objectColor(1.0f);
            Material* currentMaterial = nullptr;

            if (mesh->material) {
                objectColor = mesh->material->color;
                currentMaterial = mesh->material;
                mesh->material->Bind();
            }

            // Apply Global Illumination if enabled
            if (m_giEnabled && (!currentMaterial || currentMaterial->receiveGI)) {
                glm::mat4 view = camera.GetViewMatrix();
                glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                    (float)width / (float)height, 0.1f, 100.0f);
                m_giSystem.ApplyGI(shaderToUse, view, projection);
            }

            // Apply shadows if enabled
            if (m_shadowsEnabled) {
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
                shaderToUse->setInt("shadowMap", 6);
                shaderToUse->setMat4("lightSpaceMatrix", m_lightSpaceMatrix);
                shaderToUse->setBool("shadowsEnabled", true);
            }
            else {
                shaderToUse->setBool("shadowsEnabled", false);
            }

            // Build model matrix and other transforms
            glm::mat4 model = transform->GetModelMatrix();
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                (float)width / (float)height, 0.1f, 100.0f);

            // Set matrices
            shaderToUse->setMat4("model", model);
            shaderToUse->setMat4("view", view);
            shaderToUse->setMat4("projection", projection);

            // Calculate fragment position in light space for shadow calculation
            glm::vec3 fragPos = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glm::vec4 fragPosLightSpace = m_lightSpaceMatrix * glm::vec4(fragPos, 1.0f);
            shaderToUse->setMat4("lightMatrix", m_lightSpaceMatrix * model);

            // Set lighting uniforms
            shaderToUse->setVec3("objectColor", objectColor);
            shaderToUse->setVec3("lightColor", lightColor);
            shaderToUse->setVec3("lightPos", lightPos);
            shaderToUse->setVec3("viewPos", camera.Position);

            // Set material properties
            if (currentMaterial) {
                shaderToUse->setFloat("roughness", currentMaterial->roughness);
                shaderToUse->setFloat("metallic", currentMaterial->metallic);
                shaderToUse->setVec3("albedo", currentMaterial->albedo);
                shaderToUse->setFloat("emissive", currentMaterial->emissive);
                shaderToUse->setVec3("emissiveColor", currentMaterial->emissiveColor);
            }
            else {
                shaderToUse->setFloat("roughness", 0.5f);
                shaderToUse->setFloat("metallic", 0.0f);
                shaderToUse->setVec3("albedo", objectColor);
                shaderToUse->setFloat("emissive", 0.0f);
                shaderToUse->setVec3("emissiveColor", glm::vec3(0.0f));
            }

            // Draw mesh
            glBindVertexArray(mesh->VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
            glBindVertexArray(0);

            // Unbind material
            if (currentMaterial) {
                currentMaterial->Unbind();
            }
        }

        // Render terrain after other objects
        RenderTerrainWithGI(registry, camera, width, height, lightPos, lightColor);
    }

    void RenderTerrainWithGI(Registry& registry, Camera& camera, int width, int height,
        const glm::vec3& lightPos, const glm::vec3& lightColor) {
        auto terrainEntities = registry.GetEntitiesWith<TransformComponent, TerrainComponent>();

        for (auto e : terrainEntities) {
            auto transform = registry.GetComponent<TransformComponent>(e);
            auto terrain = registry.GetComponent<TerrainComponent>(e);

            if (!transform || !terrain || terrain->VAO == 0) continue;

            // Use terrain shader
            Shader* terrainShader = SHADER_MANAGER.GetShader(ShaderManager::PHONG);
            if (!terrainShader) continue;

            terrainShader->use();

            // Apply GI to terrain
            if (m_giEnabled) {
                glm::mat4 view = camera.GetViewMatrix();
                glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                    (float)width / (float)height, 0.1f, 100.0f);
                m_giSystem.ApplyGI(terrainShader, view, projection);
            }

            // Apply shadows to terrain
            if (m_shadowsEnabled) {
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
                terrainShader->setInt("shadowMap", 6);
                terrainShader->setMat4("lightSpaceMatrix", m_lightSpaceMatrix);
                terrainShader->setBool("shadowsEnabled", true);
            }
            else {
                terrainShader->setBool("shadowsEnabled", false);
            }

            // Set wireframe mode if enabled
            if (terrain->wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }

            // Build model matrix
            glm::mat4 model = transform->GetModelMatrix();

            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                (float)width / (float)height, 0.1f, 100.0f);

            // Set matrices
            terrainShader->setMat4("model", model);
            terrainShader->setMat4("view", view);
            terrainShader->setMat4("projection", projection);

            // Set fragment position in light space for terrain
            glm::vec3 fragPos = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glm::vec4 fragPosLightSpace = m_lightSpaceMatrix * glm::vec4(fragPos, 1.0f);
            terrainShader->setMat4("lightMatrix", m_lightSpaceMatrix * model);

            // Set lighting and material properties
            terrainShader->setVec3("objectColor", glm::vec3(0.3f, 0.6f, 0.3f)); // Green terrain
            terrainShader->setVec3("lightColor", lightColor);
            terrainShader->setVec3("lightPos", lightPos);
            terrainShader->setVec3("viewPos", camera.Position);
            terrainShader->setVec3("albedo", glm::vec3(0.3f, 0.6f, 0.3f));

            // Draw terrain
            glBindVertexArray(terrain->VAO);
            glDrawElements(GL_TRIANGLES, terrain->indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);

            // Reset polygon mode if wireframe was enabled
            if (terrain->wireframe) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        }
    }

    // Debug function to check shadow map contents
    void CheckShadowMap() {
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
        std::vector<float> depthData(m_shadowWidth * m_shadowHeight);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthData.data());

        float minDepth = 1.0f, maxDepth = 0.0f;
        int validPixels = 0;
        for (int i = 0; i < m_shadowWidth * m_shadowHeight; i++) {
            if (depthData[i] > 0.0f && depthData[i] < 1.0f) {
                validPixels++;
                if (depthData[i] < minDepth) minDepth = depthData[i];
                if (depthData[i] > maxDepth) maxDepth = depthData[i];
            }
        }

        std::cout << "Shadow Map Debug - Valid Pixels: " << validPixels
            << " Depth Range: " << minDepth << " to " << maxDepth << std::endl;

        glBindTexture(GL_TEXTURE_2D, 0);
    }
};