#include "ShaderManager.h"
#include <iostream>

void ShaderManager::LoadShaders() {
    // Load all core shaders
    shaders[PHONG] = new Shader("shaders/phong.vert", "shaders/phong.frag");
    shaders[PBR] = new Shader("shaders/pbr.vert", "shaders/pbr.frag");
    shaders[WIREFRAME] = new Shader("shaders/wireframe.vert", "shaders/wireframe.frag");
    shaders[FLAT] = new Shader("shaders/flat.vert", "shaders/flat.frag");
    shaders[UNLIT] = new Shader("shaders/unlit.vert", "shaders/unlit.frag");

    // Load GI-related shaders
    shaders[DEPTH] = new Shader("shaders/depth.vert", "shaders/depth.frag");
    shaders[GI_APPLY] = new Shader("shaders/gi_apply.vert", "shaders/gi_apply.frag");

    // Load bloom shaders
    shaders[BLOOM_BRIGHT] = new Shader("shaders/bloom_bright.vert", "shaders/bloom_bright.frag");
    shaders[BLOOM_BLUR] = new Shader("shaders/bloom_blur.vert", "shaders/bloom_blur.frag");
    shaders[BLOOM_FINAL] = new Shader("shaders/bloom_final.vert", "shaders/bloom_final.frag");

    // Voxelization shader (optional - can be nullptr if not available)
    shaders[VOXELIZATION] = nullptr;

    // Check bloom shaders
    for (int i = BLOOM_BRIGHT; i <= BLOOM_FINAL; i++) {
        ShaderType type = static_cast<ShaderType>(i);
        if (shaders[type] && shaders[type]->ID == 0) {
            std::cout << "Failed to load bloom shader: " << GetShaderName(type) << std::endl;
            delete shaders[type];
            shaders[type] = nullptr;
        }
    }

    // Check for errors
    for (auto& pair : shaders) {
        if (pair.second && pair.second->ID == 0) {
            std::cout << "Failed to load shader: " << GetShaderName(pair.first) << std::endl;
            delete pair.second;
            pair.second = nullptr;
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

Shader* ShaderManager::LoadShader(const std::string& name,
    const char* vertexPath,
    const char* fragmentPath,
    const char* geometryPath) {
    // Check if shader already exists
    auto it = namedShaders.find(name);
    if (it != namedShaders.end()) {
        return it->second;
    }

    // Create new shader - FIXED: Check if geometry path is provided
    Shader* newShader = nullptr;
    if (geometryPath && strlen(geometryPath) > 0) {
        // If you have a Shader constructor that takes geometry shader
        // newShader = new Shader(vertexPath, fragmentPath, geometryPath);
        // For now, use the 2-parameter constructor
        newShader = new Shader(vertexPath, fragmentPath);
    }
    else {
        newShader = new Shader(vertexPath, fragmentPath);
    }

    if (newShader && newShader->ID != 0) {
        namedShaders[name] = newShader;
        std::cout << "Loaded shader: " << name << std::endl;
        return newShader;
    }
    else {
        std::cout << "Failed to load shader: " << name << std::endl;
        delete newShader;
        return nullptr;
    }
}

Shader* ShaderManager::GetShader(const std::string& name) {
    auto it = namedShaders.find(name);
    if (it != namedShaders.end()) {
        return it->second;
    }

    // Also check enum shaders by name
    for (auto& pair : shaders) {
        if (GetShaderName(pair.first) == name && pair.second) {
            return pair.second;
        }
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
    case DEPTH: return "depth";
    case GI_APPLY: return "gi_apply";
    case VOXELIZATION: return "Voxelization";
    default: return "Unknown";
    case BLOOM_BRIGHT: return "Bloom Bright";
    case BLOOM_BLUR: return "Bloom Blur";
    case BLOOM_FINAL: return "Bloom Final";
    }
}

void ShaderManager::Cleanup() {
    // Cleanup enum shaders
    for (auto& pair : shaders) {
        if (pair.second) {
            delete pair.second;
            pair.second = nullptr;
        }
    }
    shaders.clear();

    // Cleanup named shaders
    for (auto& pair : namedShaders) {
        if (pair.second) {
            delete pair.second;
            pair.second = nullptr;
        }
    }
    namedShaders.clear();
}