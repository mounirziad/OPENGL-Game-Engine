#include "TextureLoader.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

unsigned int TextureLoader::LoadTexture(const std::string& path) {
    return LoadTexture(path, true);
}

unsigned int TextureLoader::LoadTexture(const std::string& path, bool flipVertical) {
    return CreateTexture(path, flipVertical);
}

unsigned int TextureLoader::CreateTexture(const std::string& path, bool flipVertical) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(flipVertical);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
            std::cout << "Unknown texture format with " << nrComponents << " components" << std::endl;
            stbi_image_free(data);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ", " << nrComponents << " channels)" << std::endl;
        stbi_image_free(data);
    }
    else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }

    return textureID;
}