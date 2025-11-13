#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Component.h"

struct TerrainComponent {
    int width = 64;
    int height = 64;
    float scale = 2.0f;
    float heightScale = 1.0f;
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
    std::vector<float> heightmap;
    bool wireframe = false;

    TerrainComponent(int w = 64, int h = 64, float s = 2.0f, float hs = 1.0f)
        : width(w), height(h), scale(s), heightScale(hs) {
    }
};