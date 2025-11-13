#include "TerrainSystem.h"
#include <iostream>
#include <cmath>
#include <random>

void TerrainSystem::GenerateTerrain(TerrainComponent& terrain) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Generate heightmap
    terrain.heightmap.resize(terrain.width * terrain.height);
    for (int z = 0; z < terrain.height; ++z) {
        for (int x = 0; x < terrain.width; ++x) {
            terrain.heightmap[z * terrain.width + x] = GenerateHeight(x, z, terrain);
        }
    }

    // Generate vertices
    for (int z = 0; z < terrain.height; ++z) {
        for (int x = 0; x < terrain.width; ++x) {
            // Position
            float xPos = (x - terrain.width / 2.0f) * terrain.scale;
            float zPos = (z - terrain.height / 2.0f) * terrain.scale;
            float yPos = terrain.heightmap[z * terrain.width + x] * terrain.heightScale;

            vertices.push_back(xPos);
            vertices.push_back(yPos);
            vertices.push_back(zPos);

            // Normal (will calculate properly later)
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
            vertices.push_back(0.0f);

            // Texture coordinates
            vertices.push_back((float)x / (terrain.width - 1));
            vertices.push_back((float)z / (terrain.height - 1));
        }
    }

    // Generate indices
    for (int z = 0; z < terrain.height - 1; ++z) {
        for (int x = 0; x < terrain.width - 1; ++x) {
            unsigned int topLeft = z * terrain.width + x;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (z + 1) * terrain.width + x;
            unsigned int bottomRight = bottomLeft + 1;

            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // Calculate normals
    for (size_t i = 0; i < indices.size(); i += 3) {
        unsigned int i1 = indices[i];
        unsigned int i2 = indices[i + 1];
        unsigned int i3 = indices[i + 2];

        glm::vec3 v1(vertices[i1 * 8], vertices[i1 * 8 + 1], vertices[i1 * 8 + 2]);
        glm::vec3 v2(vertices[i2 * 8], vertices[i2 * 8 + 1], vertices[i2 * 8 + 2]);
        glm::vec3 v3(vertices[i3 * 8], vertices[i3 * 8 + 1], vertices[i3 * 8 + 2]);

        glm::vec3 edge1 = v2 - v1;
        glm::vec3 edge2 = v3 - v1;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        // Update normals in vertices
        for (int j = 0; j < 3; ++j) {
            unsigned int idx = indices[i + j] * 8 + 3;
            vertices[idx] += normal.x;
            vertices[idx + 1] += normal.y;
            vertices[idx + 2] += normal.z;
        }
    }

    // Normalize normals
    for (size_t i = 3; i < vertices.size(); i += 8) {
        glm::vec3 normal(vertices[i], vertices[i + 1], vertices[i + 2]);
        normal = glm::normalize(normal);
        vertices[i] = normal.x;
        vertices[i + 1] = normal.y;
        vertices[i + 2] = normal.z;
    }

    SetupTerrainMesh(terrain, vertices, indices);
    terrain.vertexCount = indices.size();
    terrain.indexCount = indices.size();
}

float TerrainSystem::GenerateHeight(int x, int z, const TerrainComponent& terrain) {
    // Improved multi-octave Perlin-like noise for more interesting terrain
    float total = 0.0f;
    float frequency = 0.01f;  // Lower frequency for larger features
    float amplitude = 1.0f;
    float maxAmplitude = 0.0f;
    float persistence = 0.5f; // How much each octave contributes

    // 6 octaves for detailed terrain with hills and valleys
    for (int i = 0; i < 6; ++i) {
        // Sample coordinates with frequency
        float sampleX = x * frequency;
        float sampleZ = z * frequency;

        // Improved noise function with multiple frequencies
        float noise = 0.0f;

        // Base noise - creates large hills
        noise += std::sin(sampleX * 1.0f) * std::cos(sampleZ * 1.0f);

        // Secondary noise - adds medium details
        noise += 0.5f * std::sin(sampleX * 2.3f + sampleZ * 1.7f) *
            std::cos(sampleZ * 2.1f - sampleX * 1.3f);

        // Tertiary noise - adds small details and roughness
        noise += 0.25f * std::sin(sampleX * 4.7f) * std::cos(sampleZ * 3.9f) *
            std::sin(sampleX * 1.9f + sampleZ * 2.8f);

        // Ridge noise - creates sharper hilltops
        float ridgeNoise = 1.0f - std::abs(std::sin(sampleX * 1.5f) * std::cos(sampleZ * 1.2f));
        noise += 0.3f * ridgeNoise * ridgeNoise; // Square it for sharper ridges

        // Domain warping for more organic shapes
        float warpX = sampleX + 0.5f * noise;
        float warpZ = sampleZ + 0.5f * noise;
        float warpedNoise = std::sin(warpX * 0.8f) * std::cos(warpZ * 0.8f);
        noise += 0.2f * warpedNoise;

        total += noise * amplitude;
        maxAmplitude += amplitude;
        amplitude *= persistence;  // Reduce amplitude for each octave
        frequency *= 2.0f;         // Double frequency for each octave
    }

    // Normalize and add some bias for more hills than valleys
    float height = total / maxAmplitude;

    // Apply a power curve to make hills more prominent
    if (height > 0) {
        height = std::pow(height, 0.7f); // Makes hills sharper
    }
    else {
        height = -std::pow(-height, 1.3f); // Makes valleys smoother
    }

    return height;
}

void TerrainSystem::SetupTerrainMesh(TerrainComponent& terrain,
    const std::vector<float>& vertices,
    const std::vector<unsigned int>& indices) {
    // Clean up existing buffers
    CleanupTerrain(terrain);

    glGenVertexArrays(1, &terrain.VAO);
    glGenBuffers(1, &terrain.VBO);
    glGenBuffers(1, &terrain.EBO);

    glBindVertexArray(terrain.VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, terrain.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
        vertices.data(), GL_STATIC_DRAW);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    // Vertex attributes
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinates
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void TerrainSystem::RenderTerrain(Registry& registry, Camera& camera, int width, int height,
    const glm::vec3& lightPos, const glm::vec3& lightColor) {
    auto entities = registry.GetEntitiesWith<TransformComponent, TerrainComponent>();

    for (auto entity : entities) {
        auto transform = registry.GetComponent<TransformComponent>(entity);
        auto terrain = registry.GetComponent<TerrainComponent>(entity);

        if (!transform || !terrain || terrain->VAO == 0) continue;

        // Use terrain shader (you might want to create a dedicated terrain shader)
        Shader* terrainShader = SHADER_MANAGER.GetShader(ShaderManager::PHONG);
        if (!terrainShader) continue;

        terrainShader->use();

        // Set wireframe mode if enabled
        if (terrain->wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

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
        terrainShader->setMat4("model", model);
        terrainShader->setMat4("view", view);
        terrainShader->setMat4("projection", projection);

        // Set lighting and material properties
        terrainShader->setVec3("objectColor", glm::vec3(0.3f, 0.6f, 0.3f)); // Green terrain
        terrainShader->setVec3("lightColor", lightColor);
        terrainShader->setVec3("lightPos", lightPos);
        terrainShader->setVec3("viewPos", camera.Position);

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

void TerrainSystem::CleanupTerrain(TerrainComponent& terrain) {
    if (terrain.VAO) {
        glDeleteVertexArrays(1, &terrain.VAO);
        terrain.VAO = 0;
    }
    if (terrain.VBO) {
        glDeleteBuffers(1, &terrain.VBO);
        terrain.VBO = 0;
    }
    if (terrain.EBO) {
        glDeleteBuffers(1, &terrain.EBO);
        terrain.EBO = 0;
    }
}