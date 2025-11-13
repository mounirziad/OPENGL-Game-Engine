#include "ShaderManager.h"
#include <iostream>

void ShaderManager::LoadShaders() {
    // Load all shaders
    shaders[PHONG] = new Shader("shaders/phong.vert", "shaders/phong.frag");
    shaders[PBR] = new Shader("shaders/pbr.vert", "shaders/pbr.frag");
    shaders[WIREFRAME] = new Shader("shaders/wireframe.vert", "shaders/wireframe.frag");
    shaders[FLAT] = new Shader("shaders/flat.vert", "shaders/flat.frag");
    shaders[UNLIT] = new Shader("shaders/unlit.vert", "shaders/unlit.frag");

    // Check for errors
    for (auto& pair : shaders) {
        if (!pair.second || pair.second->ID == 0) {
            std::cout << "Failed to load shader: " << GetShaderName(pair.first) << std::endl;
        }
    }
}

Shader* ShaderManager::GetShader(ShaderType type) {
    auto it = shaders.find(type);
    if (it != shaders.end()) {
        return it->second;
    }
    return nullptr;
}

const char* ShaderManager::GetShaderName(ShaderType type) {
    switch (type) {
    case PHONG: return "Phong";
    case PBR: return "PBR";
    case WIREFRAME: return "Wireframe";
    case FLAT: return "Flat";
    case UNLIT: return "Unlit";
    default: return "Unknown";
    }
}