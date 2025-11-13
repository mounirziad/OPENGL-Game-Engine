#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

// Forward declarations
class Shader;
struct Material;

// Transform Component - handles position, rotation, scale
struct TransformComponent {
    glm::vec3 position;
    glm::vec3 rotation;    // Euler angles in degrees
    glm::vec3 scale;

    TransformComponent(glm::vec3 pos = { 0.0f, 0.0f, 0.0f },
        glm::vec3 rot = { 0.0f, 0.0f, 0.0f },
        glm::vec3 scl = { 1.0f, 1.0f, 1.0f })
        : position(pos), rotation(rot), scale(scl) {
    }

    // Get the model matrix for rendering
    glm::mat4 GetModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }

    // Utility functions
    void Translate(const glm::vec3& translation) {
        position += translation;
    }

    void Rotate(const glm::vec3& rotationDelta) {
        rotation += rotationDelta;
    }

    void Scale(const glm::vec3& scaleFactor) {
        scale *= scaleFactor;
    }
};

// Mesh Component - handles rendering geometry
struct MeshComponent {
    unsigned int VAO;
    unsigned int vertexCount;
    Shader* shader;
    Material* material = nullptr;

    MeshComponent() : VAO(0), vertexCount(0), shader(nullptr), material(nullptr) {}
};

// Physics Component - handles movement and forces
struct PhysicsComponent {
    glm::vec3 velocity = { 0.0f, 0.0f, 0.0f };
    glm::vec3 acceleration = { 0.0f, -9.81f, 0.0f }; // Default gravity
    float mass = 1.0f;
    bool useGravity = true;
    bool isGrounded = false;
    float restitution = 0.3f; // Bounciness
    float friction = 0.8f;

    PhysicsComponent(bool gravity = true, float m = 1.0f)
        : useGravity(gravity), mass(m) {
    }
};

// Collider Types
enum class ColliderType {
    SPHERE,
    BOX,
    MESH
};

// Collider Component - handles collision detection
struct ColliderComponent {
    ColliderType type = ColliderType::SPHERE;
    glm::vec3 center = { 0.0f, 0.0f, 0.0f };
    glm::vec3 size = { 1.0f, 1.0f, 1.0f }; // For box collider
    float radius = 0.5f; // For sphere collider

    ColliderComponent(ColliderType colliderType = ColliderType::SPHERE)
        : type(colliderType) {
    }
};

// Terrain Component - handles procedural terrain
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