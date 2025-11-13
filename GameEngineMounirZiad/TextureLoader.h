#pragma once
#include <glad/glad.h>
#include <string>

class TextureLoader {
public:
    static unsigned int LoadTexture(const std::string& path);
    static unsigned int LoadTexture(const std::string& path, bool flipVertical);

private:
    static unsigned int CreateTexture(const std::string& path, bool flipVertical);
};