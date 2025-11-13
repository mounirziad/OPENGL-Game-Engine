#pragma once
#include "Registry.h"
#include "Component.h"
#include <glm/glm.hpp>
#include <vector>

class PhysicsSystem {
public:
    void Update(Registry& registry, float deltaTime);
    void SetGravity(const glm::vec3& gravity) { this->gravity = gravity; }

private:
    glm::vec3 gravity = { 0.0f, -9.81f, 0.0f };

    void ApplyGravity(PhysicsComponent& physics, float deltaTime);
    void Integrate(TransformComponent& transform, PhysicsComponent& physics, float deltaTime);

    // Object-to-object collision handling
    void HandleObjectCollisions(Registry& registry, std::vector<Entity>& entities);
    bool CheckSphereSphereCollision(const TransformComponent& transformA, const ColliderComponent& colliderA,
        const TransformComponent& transformB, const ColliderComponent& colliderB,
        glm::vec3& collisionNormal, float& penetrationDepth);
    bool CheckBoxBoxCollision(const TransformComponent& transformA, const ColliderComponent& colliderA,
        const TransformComponent& transformB, const ColliderComponent& colliderB,
        glm::vec3& collisionNormal, float& penetrationDepth);
    void ResolveCollision(TransformComponent& transformA, PhysicsComponent& physicsA, ColliderComponent& colliderA,
        TransformComponent& transformB, PhysicsComponent& physicsB, ColliderComponent& colliderB,
        const glm::vec3& collisionNormal, float penetrationDepth);

    // Terrain collision handling
    void HandleTerrainCollision(Registry& registry, Entity entity, TransformComponent& transform,
        PhysicsComponent& physics, ColliderComponent& collider,
        TerrainComponent& terrain);
    float GetTerrainHeightAt(const TerrainComponent& terrain, const TransformComponent& terrainTransform,
        float worldX, float worldZ);
    bool CheckSphereTerrainCollision(const TransformComponent& transform,
        const ColliderComponent& collider,
        const TerrainComponent& terrain,
        const TransformComponent& terrainTransform,
        float& terrainHeight);
};