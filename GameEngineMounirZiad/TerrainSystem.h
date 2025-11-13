#pragma once
#include "Registry.h"
#include "TerrainComponent.h"
#include "ShaderManager.h"
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "Camera.h"

class TerrainSystem {
public:
    void GenerateTerrain(TerrainComponent& terrain);
    void RenderTerrain(Registry& registry, Camera& camera, int width, int height,
        const glm::vec3& lightPos, const glm::vec3& lightColor);
    void CleanupTerrain(TerrainComponent& terrain);

private:
    float GenerateHeight(int x, int z, const TerrainComponent& terrain);
    void SetupTerrainMesh(TerrainComponent& terrain,
        const std::vector<float>& vertices,
        const std::vector<unsigned int>& indices);
};