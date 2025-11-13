#pragma once
#include <glm/glm.hpp>

// Forward declarations
class Shader;
struct Material;

// Component details what entities have, i.e. position, rotation and scale
struct TransformComponent {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    TransformComponent(glm::vec3 pos = { 0,0,0 },
        glm::vec3 rot = { 0,0,0 },
        glm::vec3 scl = { 1,1,1 })
        : position(pos), rotation(rot), scale(scl) {
    }
};

struct MeshComponent {
    unsigned int VAO;
    unsigned int vertexCount;
    Shader* shader;
    Material* material = nullptr;  // Initialize to nullptr

    MeshComponent() : VAO(0), vertexCount(0), shader(nullptr), material(nullptr) {}
};