#pragma once
#include "ShaderManager.h"
#include <glad/glad.h>
#include <vector>

class BloomSystem {
private:
    unsigned int hdrFBO;
    unsigned int colorBuffers[2]; // 0: scene, 1: bright areas
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    unsigned int quadVAO, quadVBO;

    bool bloomEnabled = true;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.5f;
    float blurStrength = 1.0f;
    int blurIterations = 10;

public:
    BloomSystem() : hdrFBO(0), quadVAO(0), quadVBO(0) {}

    bool Initialize(int width, int height);
    void Cleanup();

    void BeginSceneCapture();
    void EndSceneCapture();
    void ApplyBloom(int width, int height);

    void SetBloomEnabled(bool enabled) { bloomEnabled = enabled; }
    void SetBloomThreshold(float threshold) { bloomThreshold = threshold; }
    void SetBloomIntensity(float intensity) { bloomIntensity = intensity; }
    void SetBlurStrength(float strength) { blurStrength = strength; }
    void SetBlurIterations(int iterations) { blurIterations = iterations; }

    bool IsBloomEnabled() const { return bloomEnabled; }

private:
    void RenderQuad();
    void SetupQuad();
};