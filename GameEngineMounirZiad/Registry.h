#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "Entity.h"
#include "Component.h"  // This must include TransformComponent and MeshComponent
#include "TerrainComponent.h"  // Add this include

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

    // Simple DestroyEntity implementation
    void DestroyEntity(Entity entity) {
        // Remove from all component storages
        transformComponents.erase(entity);
        meshComponents.erase(entity);
        terrainComponents.erase(entity);  // Add this line
    }

private:
    Entity nextEntityId = 0;

    // Explicit storage for each component type
    std::unordered_map<Entity, TransformComponent> transformComponents;
    std::unordered_map<Entity, MeshComponent> meshComponents;
    std::unordered_map<Entity, TerrainComponent> terrainComponents;  // Add this line

    template<typename T>
    std::unordered_map<Entity, T>& GetComponentStorage() {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            return transformComponents;
        }
        else if constexpr (std::is_same_v<T, MeshComponent>) {
            return meshComponents;
        }
        else if constexpr (std::is_same_v<T, TerrainComponent>) {  // Add this condition
            return terrainComponents;
        }
        else {
            static_assert(sizeof(T) == 0, "Unknown component type");
        }
    }
};