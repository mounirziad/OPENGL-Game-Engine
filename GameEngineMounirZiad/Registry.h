#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "Entity.h"
#include "Component.h"  // Include the single component file

class Registry {
public:
    Entity CreateEntity() {
        Entity id = nextEntityId++;
        return id;
    }

    template<typename T>
    void AddComponent(Entity entity, T component) {
        GetComponentStorage<T>()[entity] = component;
    }

    template<typename T>
    T* GetComponent(Entity entity) {
        auto& storage = GetComponentStorage<T>();
        auto it = storage.find(entity);
        if (it != storage.end())
            return &it->second;
        return nullptr;
    }

    template<typename T>
    bool HasComponent(Entity entity) {
        auto& storage = GetComponentStorage<T>();
        return storage.find(entity) != storage.end();
    }

    template<typename... ComponentTypes>
    std::vector<Entity> GetEntitiesWith() {
        std::vector<Entity> entities;

        if constexpr (sizeof...(ComponentTypes) > 0) {
            using FirstComponent = typename std::tuple_element<0, std::tuple<ComponentTypes...>>::type;
            auto& firstStorage = GetComponentStorage<FirstComponent>();
            for (auto& pair : firstStorage) {
                entities.push_back(pair.first);
            }

            // Filter entities that have all required components
            entities.erase(std::remove_if(entities.begin(), entities.end(),
                [this](Entity entity) {
                    return (!HasComponent<ComponentTypes>(entity) || ...);
                }), entities.end());
        }

        return entities;
    }

    // Single DestroyEntity implementation
    void DestroyEntity(Entity entity) {
        transformComponents.erase(entity);
        meshComponents.erase(entity);
        terrainComponents.erase(entity);
        physicsComponents.erase(entity);
        colliderComponents.erase(entity);
    }

private:
    Entity nextEntityId = 0;

    // Explicit storage for each component type
    std::unordered_map<Entity, TransformComponent> transformComponents;
    std::unordered_map<Entity, MeshComponent> meshComponents;
    std::unordered_map<Entity, TerrainComponent> terrainComponents;
    std::unordered_map<Entity, PhysicsComponent> physicsComponents;
    std::unordered_map<Entity, ColliderComponent> colliderComponents;

    template<typename T>
    std::unordered_map<Entity, T>& GetComponentStorage() {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            return transformComponents;
        }
        else if constexpr (std::is_same_v<T, MeshComponent>) {
            return meshComponents;
        }
        else if constexpr (std::is_same_v<T, TerrainComponent>) {
            return terrainComponents;
        }
        else if constexpr (std::is_same_v<T, PhysicsComponent>) {
            return physicsComponents;
        }
        else if constexpr (std::is_same_v<T, ColliderComponent>) {
            return colliderComponents;
        }
        else {
            static_assert(sizeof(T) == 0, "Unknown component type");
        }
    }
};