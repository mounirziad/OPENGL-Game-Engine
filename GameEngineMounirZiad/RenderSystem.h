#pragma once
#include "Registry.h"
#include "Component.h"
#include "Material.h"
#include "ShaderManager.h"
#include <glm/gtc/matrix_transform.hpp>

class RenderSystem {
public:
    void Render(Registry& registry, Camera& camera, int width, int height,
        const glm::vec3& lightPos, const glm::vec3& lightColor)
    {
        auto entities = registry.GetEntitiesWith<TransformComponent, MeshComponent>();
        for (auto e : entities) {
            auto transform = registry.GetComponent<TransformComponent>(e);
            auto mesh = registry.GetComponent<MeshComponent>(e);

            if (!transform || !mesh) continue;

            // Use material shader if available, otherwise default to mesh shader
            Shader* shaderToUse = mesh->shader;
            glm::vec3 objectColor(1.0f);

            // Store material reference for cleanup
            Material* currentMaterial = nullptr;

            if (mesh->material && mesh->material->shader) {
                shaderToUse = mesh->material->shader;
                objectColor = mesh->material->color;
                currentMaterial = mesh->material;

                // Bind material (this handles textures, wireframe, etc.)
                mesh->material->Bind();
            }

            if (!shaderToUse) continue;

            shaderToUse->use();

            // Build model matrix
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, transform->position);
            model = glm::rotate(model, glm::radians(transform->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(transform->rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(transform->rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, transform->scale);

            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                (float)width / (float)height, 0.1f, 100.0f);

            // Set matrices
            shaderToUse->setMat4("model", model);
            shaderToUse->setMat4("view", view);
            shaderToUse->setMat4("projection", projection);

            // Set lighting uniforms (only for shaders that use them)
            // Note: These might be overridden by material if it uses textures
            shaderToUse->setVec3("objectColor", objectColor);
            shaderToUse->setVec3("lightColor", lightColor);
            shaderToUse->setVec3("lightPos", lightPos);
            shaderToUse->setVec3("viewPos", camera.Position);

            // Set PBR parameters (if material doesn't already set them)
            if (!currentMaterial) {
                shaderToUse->setFloat("roughness", 0.5f);
                shaderToUse->setFloat("metallic", 0.0f);
            }

            // Draw mesh
            glBindVertexArray(mesh->VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);

            // Unbind material to reset any OpenGL state changes
            if (currentMaterial) {
                currentMaterial->Unbind();
            }
        }
    }
};