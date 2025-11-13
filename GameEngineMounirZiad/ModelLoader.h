#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Component.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <glad/glad.h>

class ModelLoader {
public:
    struct MeshData {
        unsigned int VAO;
        unsigned int VBO;
        unsigned int vertexCount;
        bool hasTexCoords;
    };

    static bool LoadOBJ(const std::string& filename,
        unsigned int& VAO,
        unsigned int& vertexCount) {
        return LoadOBJWithTexCoords(filename, VAO, vertexCount);
    }

    static bool LoadOBJWithTexCoords(const std::string& filename,
        unsigned int& VAO,
        unsigned int& vertexCount) {
        // Load OBJ file using tinyobjloader
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
            std::cout << "Failed to load OBJ: " << warn << err << std::endl;
            return false;
        }

        std::vector<float> vertices;
        bool hasTexCoords = !attrib.texcoords.empty();

        // Combine all shapes into single vertex array
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                // Position
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
                vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

                // Normal (if available)
                if (index.normal_index >= 0) {
                    vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                    vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                    vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
                }
                else {
                    // Default normal if not provided
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(0.0f);
                }

                // Texture coordinates (if available)
                if (hasTexCoords && index.texcoord_index >= 0) {
                    vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                    vertices.push_back(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]); // Flip Y coordinate
                }
                else {
                    // Default UV if not provided
                    vertices.push_back(0.0f);
                    vertices.push_back(0.0f);
                }
            }
        }

        // Create VAO from vertices
        int stride = hasTexCoords ? 8 : 6; // 8 floats per vertex if texture coords exist (position + normal + texcoord)
        VAO = CreateVAO(vertices, stride);
        vertexCount = vertices.size() / stride;

        std::cout << "Loaded model: " << filename << " with " << vertexCount << " vertices" << std::endl;
        std::cout << "Has texture coordinates: " << (hasTexCoords ? "Yes" : "No") << std::endl;
        return true;
    }

    static bool LoadOBJ(const std::string& filename, MeshData& meshData) {
        return LoadOBJWithTexCoords(filename, meshData.VAO, meshData.vertexCount);
    }

private:
    static unsigned int CreateVAO(const std::vector<float>& vertices, int stride) {
        unsigned int VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
            vertices.data(), GL_STATIC_DRAW);

        // Position attribute (location = 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal attribute (location = 1)  
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Texture coordinate attribute (location = 2) - only if stride is 8
        if (stride == 8) {
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
        }

        glBindVertexArray(0); // Unbind VAO

        return VAO;
    }
};