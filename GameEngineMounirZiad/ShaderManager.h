#pragma once
#include "Shader.h"
#include <unordered_map>
#include <string>

class ShaderManager {
public:
    enum ShaderType {
        PHONG,
        PBR,
        WIREFRAME,
        FLAT,
        UNLIT
    };

    static ShaderManager& GetInstance() {
        static ShaderManager instance;
        return instance;
    }

    void LoadShaders();
    Shader* GetShader(ShaderType type);
    const char* GetShaderName(ShaderType type);

private:
    ShaderManager() = default;
    std::unordered_map<ShaderType, Shader*> shaders;
};

// Helper macro
#define SHADER_MANAGER ShaderManager::GetInstance()