#pragma once
#include "Shader.h"
#include <unordered_map>
#include <string>
#include <iostream>

class ShaderManager {
public:
    enum ShaderType {
        PHONG,
        PBR,
        WIREFRAME,
        FLAT,
        UNLIT,
        DEPTH,           // Add depth shader
        GI_APPLY,        // Add GI apply shader
        VOXELIZATION,     // Add voxelization shader
        BLOOM_BRIGHT,    // Add this
        BLOOM_BLUR,      // Add this  
        BLOOM_FINAL
    };

    static ShaderManager& GetInstance() {
        static ShaderManager instance;
        return instance;
    }

    void LoadShaders();
    Shader* GetShader(ShaderType type);
    
    // NEW: Load shader by name (for dynamic loading)
    Shader* LoadShader(const std::string& name, 
                      const char* vertexPath, 
                      const char* fragmentPath, 
                      const char* geometryPath = nullptr);
    
    Shader* GetShader(const std::string& name);
    const char* GetShaderName(ShaderType type);

    // Cleanup to prevent memory leaks
    void Cleanup();

private:
    ShaderManager() = default;
    ~ShaderManager() { Cleanup(); }
    
    std::unordered_map<ShaderType, Shader*> shaders;
    std::unordered_map<std::string, Shader*> namedShaders; // NEW: For dynamic shader loading
};

// Helper macro
#define SHADER_MANAGER ShaderManager::GetInstance()