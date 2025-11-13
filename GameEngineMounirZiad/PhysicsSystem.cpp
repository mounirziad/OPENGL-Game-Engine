#include "PhysicsSystem.h"
#include <iostream>
#include <algorithm>

void PhysicsSystem::Update(Registry& registry, float deltaTime) {
    auto entities = registry.GetEntitiesWith<TransformComponent, PhysicsComponent>();

    // First update all physics
    for (auto entity : entities) {
        auto transform = registry.GetComponent<TransformComponent>(entity);
        auto physics = registry.GetComponent<PhysicsComponent>(entity);
        auto collider = registry.GetComponent<ColliderComponent>(entity);

        if (!transform || !physics) continue;

        // Apply gravity
        if (physics->useGravity && !physics->isGrounded) {
            ApplyGravity(*physics, deltaTime);
        }

        // Integrate position
        Integrate(*transform, *physics, deltaTime);

        // Handle collisions with terrain
        auto terrainEntities = registry.GetEntitiesWith<TerrainComponent, TransformComponent>();
        for (auto terrainEntity : terrainEntities) {
            auto terrain = registry.GetComponent<TerrainComponent>(terrainEntity);
            auto terrainTransform = registry.GetComponent<TransformComponent>(terrainEntity);
            if (terrain && terrainTransform && collider) {
                HandleTerrainCollision(registry, entity, *transform, *physics, *collider, *terrain);
            }
        }
    }

    // Then handle object-to-object collisions
    HandleObjectCollisions(registry, entities);
}

void PhysicsSystem::ApplyGravity(PhysicsComponent& physics, float deltaTime) {
    physics.velocity += gravity * deltaTime;
}

void PhysicsSystem::Integrate(TransformComponent& transform, PhysicsComponent& physics, float deltaTime) {
    transform.position += physics.velocity * deltaTime;
}

void PhysicsSystem::HandleObjectCollisions(Registry& registry, std::vector<Entity>& entities) {
    // Check all pairs of entities for collisions
    for (size_t i = 0; i < entities.size(); ++i) {
        for (size_t j = i + 1; j < entities.size(); ++j) {
            Entity entityA = entities[i];
            Entity entityB = entities[j];

            auto transformA = registry.GetComponent<TransformComponent>(entityA);
            auto physicsA = registry.GetComponent<PhysicsComponent>(entityA);
            auto colliderA = registry.GetComponent<ColliderComponent>(entityA);

            auto transformB = registry.GetComponent<TransformComponent>(entityB);
            auto physicsB = registry.GetComponent<PhysicsComponent>(entityB);
            auto colliderB = registry.GetComponent<ColliderComponent>(entityB);

            if (!transformA || !physicsA || !colliderA || !transformB || !physicsB || !colliderB) {
                continue;
            }

            // Check for collision based on collider types
            bool isColliding = false;
            glm::vec3 collisionNormal;
            float penetrationDepth = 0.0f;

            if (colliderA->type == ColliderType::SPHERE && colliderB->type == ColliderType::SPHERE) {
                isColliding = CheckSphereSphereCollision(*transformA, *colliderA, *transformB, *colliderB,
                    collisionNormal, penetrationDepth);
            }
            else if (colliderA->type == ColliderType::BOX && colliderB->type == ColliderType::BOX) {
                isColliding = CheckBoxBoxCollision(*transformA, *colliderA, *transformB, *colliderB,
                    collisionNormal, penetrationDepth);
            }
            else {
                // Mixed colliders - use sphere approximation for simplicity
                isColliding = CheckSphereSphereCollision(*transformA, *colliderA, *transformB, *colliderB,
                    collisionNormal, penetrationDepth);
            }

            if (isColliding) {
                ResolveCollision(*transformA, *physicsA, *colliderA,
                    *transformB, *physicsB, *colliderB,
                    collisionNormal, penetrationDepth);
            }
        }
    }
}

bool PhysicsSystem::CheckSphereSphereCollision(const TransformComponent& transformA, const ColliderComponent& colliderA,
    const TransformComponent& transformB, const ColliderComponent& colliderB,
    glm::vec3& collisionNormal, float& penetrationDepth) {
    glm::vec3 delta = transformB.position - transformA.position;
    float distance = glm::length(delta);
    float minDistance = colliderA.radius + colliderB.radius;

    if (distance < minDistance) {
        collisionNormal = glm::normalize(delta);
        penetrationDepth = minDistance - distance;
        return true;
    }
    return false;
}

bool PhysicsSystem::CheckBoxBoxCollision(const TransformComponent& transformA, const ColliderComponent& colliderA,
    const TransformComponent& transformB, const ColliderComponent& colliderB,
    glm::vec3& collisionNormal, float& penetrationDepth) {
    // Calculate half sizes
    glm::vec3 halfSizeA = colliderA.size * transformA.scale * 0.5f;
    glm::vec3 halfSizeB = colliderB.size * transformB.scale * 0.5f;

    // Calculate distance between centers
    glm::vec3 delta = transformB.position - transformA.position;

    // Check for overlap on each axis
    glm::vec3 overlap;
    overlap.x = halfSizeA.x + halfSizeB.x - std::abs(delta.x);
    overlap.y = halfSizeA.y + halfSizeB.y - std::abs(delta.y);
    overlap.z = halfSizeA.z + halfSizeB.z - std::abs(delta.z);

    // If overlap on all axes, we have a collision
    if (overlap.x > 0 && overlap.y > 0 && overlap.z > 0) {
        // Find the axis with minimum penetration
        if (overlap.x < overlap.y && overlap.x < overlap.z) {
            penetrationDepth = overlap.x;
            collisionNormal = glm::vec3(delta.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
        }
        else if (overlap.y < overlap.z) {
            penetrationDepth = overlap.y;
            collisionNormal = glm::vec3(0.0f, delta.y > 0 ? 1.0f : -1.0f, 0.0f);
        }
        else {
            penetrationDepth = overlap.z;
            collisionNormal = glm::vec3(0.0f, 0.0f, delta.z > 0 ? 1.0f : -1.0f);
        }
        return true;
    }
    return false;
}

void PhysicsSystem::ResolveCollision(TransformComponent& transformA, PhysicsComponent& physicsA, ColliderComponent& colliderA,
    TransformComponent& transformB, PhysicsComponent& physicsB, ColliderComponent& colliderB,
    const glm::vec3& collisionNormal, float penetrationDepth) {
    // Separate the objects
    float totalMass = physicsA.mass + physicsB.mass;
    if (totalMass > 0) {
        float ratioA = physicsB.mass / totalMass;
        float ratioB = physicsA.mass / totalMass;

        transformA.position -= collisionNormal * penetrationDepth * ratioA;
        transformB.position += collisionNormal * penetrationDepth * ratioB;
    }

    // Calculate relative velocity
    glm::vec3 relativeVelocity = physicsB.velocity - physicsA.velocity;
    float velocityAlongNormal = glm::dot(relativeVelocity, collisionNormal);

    // Only resolve if objects are moving towards each other
    if (velocityAlongNormal > 0) return;

    // Calculate restitution (bounciness)
    float restitution = std::min(physicsA.restitution, physicsB.restitution);

    // Calculate impulse scalar
    float impulseScalar = -(1.0f + restitution) * velocityAlongNormal;
    impulseScalar /= physicsA.mass + physicsB.mass;

    // Apply impulse
    glm::vec3 impulse = collisionNormal * impulseScalar;
    physicsA.velocity -= impulse * physicsB.mass;
    physicsB.velocity += impulse * physicsA.mass;

    // Apply friction
    glm::vec3 tangent = relativeVelocity - collisionNormal * velocityAlongNormal;
    if (glm::length(tangent) > 0.001f) {
        tangent = glm::normalize(tangent);
        float friction = std::min(physicsA.friction, physicsB.friction);
        float frictionImpulse = glm::dot(relativeVelocity, tangent) * friction;

        glm::vec3 frictionVector = tangent * frictionImpulse;
        physicsA.velocity += frictionVector * physicsB.mass;
        physicsB.velocity -= frictionVector * physicsA.mass;
    }

    // Update grounded state if collision is from below
    if (collisionNormal.y > 0.7f) {
        if (physicsA.velocity.y <= 0) physicsA.isGrounded = true;
        if (physicsB.velocity.y <= 0) physicsB.isGrounded = true;
    }
}

void PhysicsSystem::HandleTerrainCollision(Registry& registry, Entity entity, TransformComponent& transform,
    PhysicsComponent& physics, ColliderComponent& collider,
    TerrainComponent& terrain) {
    // Get terrain entity's transform
    auto terrainEntities = registry.GetEntitiesWith<TerrainComponent, TransformComponent>();
    if (terrainEntities.empty()) return;

    auto terrainEntity = terrainEntities[0];
    auto terrainTransform = registry.GetComponent<TransformComponent>(terrainEntity);
    if (!terrainTransform) return;

    float terrainHeight = 0.0f;
    bool isColliding = false;

    switch (collider.type) {
    case ColliderType::SPHERE:
        isColliding = CheckSphereTerrainCollision(transform, collider, terrain, *terrainTransform, terrainHeight);
        break;
    case ColliderType::BOX:
        // Simple box collision - use sphere approximation for now
        isColliding = CheckSphereTerrainCollision(transform, collider, terrain, *terrainTransform, terrainHeight);
        break;
    case ColliderType::MESH:
        // For complex meshes, use sphere approximation
        isColliding = CheckSphereTerrainCollision(transform, collider, terrain, *terrainTransform, terrainHeight);
        break;
    }

    if (isColliding) {
        float objectBottom = transform.position.y - collider.radius;

        if (objectBottom <= terrainHeight && physics.velocity.y < 0) {
            // Collision response
            transform.position.y = terrainHeight + collider.radius;
            physics.velocity.y = -physics.velocity.y * physics.restitution; // Bounce
            physics.isGrounded = true;

            // Apply friction
            physics.velocity.x *= physics.friction;
            physics.velocity.z *= physics.friction;

            // Stop very small bounces
            if (std::abs(physics.velocity.y) < 0.1f) {
                physics.velocity.y = 0.0f;
            }
        }
    }
    else {
        physics.isGrounded = false;
    }
}

bool PhysicsSystem::CheckSphereTerrainCollision(const TransformComponent& transform,
    const ColliderComponent& collider,
    const TerrainComponent& terrain,
    const TransformComponent& terrainTransform,
    float& terrainHeight) {
    // Get terrain height at this position
    terrainHeight = GetTerrainHeightAt(terrain, terrainTransform, transform.position.x, transform.position.z);

    // Check if sphere is colliding with terrain
    float sphereBottom = transform.position.y - collider.radius;
    return sphereBottom <= terrainHeight;
}

float PhysicsSystem::GetTerrainHeightAt(const TerrainComponent& terrain,
    const TransformComponent& terrainTransform,
    float worldX, float worldZ) {
    // Adjust for terrain's world position
    float localX = worldX - terrainTransform.position.x;
    float localZ = worldZ - terrainTransform.position.z;

    // Convert to terrain grid coordinates
    float gridX = (localX / terrain.scale) + (terrain.width / 2.0f);
    float gridZ = (localZ / terrain.scale) + (terrain.height / 2.0f);

    // Clamp to terrain bounds
    int x0 = std::max(0, std::min(terrain.width - 1, static_cast<int>(gridX)));
    int z0 = std::max(0, std::min(terrain.height - 1, static_cast<int>(gridZ)));

    // Simple bilinear interpolation for smoother height sampling
    int x1 = std::min(terrain.width - 1, x0 + 1);
    int z1 = std::min(terrain.height - 1, z0 + 1);

    float fracX = gridX - x0;
    float fracZ = gridZ - z0;

    // Get heights at the four corners
    float h00 = terrain.heightmap[z0 * terrain.width + x0] * terrain.heightScale;
    float h10 = terrain.heightmap[z0 * terrain.width + x1] * terrain.heightScale;
    float h01 = terrain.heightmap[z1 * terrain.width + x0] * terrain.heightScale;
    float h11 = terrain.heightmap[z1 * terrain.width + x1] * terrain.heightScale;

    // Bilinear interpolation
    float height = h00 * (1 - fracX) * (1 - fracZ) +
        h10 * fracX * (1 - fracZ) +
        h01 * (1 - fracX) * fracZ +
        h11 * fracX * fracZ;

    // Add terrain's world position
    height += terrainTransform.position.y;

    return height;
}