#include "BloomSystem.h"
#include <iostream>

bool BloomSystem::Initialize(int width, int height) {
    // Setup framebuffer for HDR rendering
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // Create two color buffers (one for normal rendering, one for brightness threshold)
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // Create and attach depth buffer
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // Tell OpenGL which color attachments we'll use for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Framebuffer not complete!" << std::endl;
        return false;
    }

    // Ping-pong framebuffers for blurring
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cout << "Pingpong framebuffer not complete!" << std::endl;
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Setup screen quad
    SetupQuad();

    std::cout << "Bloom system initialized successfully" << std::endl;
    return true;
}

void BloomSystem::Cleanup() {
    if (hdrFBO) glDeleteFramebuffers(1, &hdrFBO);
    if (colorBuffers[0]) glDeleteTextures(2, colorBuffers);
    if (pingpongFBO[0]) glDeleteFramebuffers(2, pingpongFBO);
    if (pingpongColorbuffers[0]) glDeleteTextures(2, pingpongColorbuffers);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void BloomSystem::BeginSceneCapture() {
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void BloomSystem::EndSceneCapture() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomSystem::ApplyBloom(int width, int height) {
    if (!bloomEnabled) {
        // Just render the scene without bloom
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader* finalShader = SHADER_MANAGER.GetShader(ShaderManager::BLOOM_FINAL);
        if (!finalShader) return;

        finalShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        finalShader->setInt("scene", 0);
        finalShader->setFloat("bloomIntensity", 0.0f); // No bloom

        RenderQuad();
        return;
    }

    // 1. Use the EMISSIVE buffer (colorBuffers[1]) for bloom, not extract bright areas
    // The emissive buffer should already contain only emissive objects
    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Just copy the emissive buffer to pingpong - no thresholding needed
    Shader* copyShader = SHADER_MANAGER.GetShader(ShaderManager::BLOOM_BRIGHT); // Reuse this shader
    if (copyShader) {
        copyShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[1]); // Use EMISSIVE buffer
        copyShader->setInt("scene", 0);
        copyShader->setFloat("bloomThreshold", 0.0f); // No threshold - use all emissive data

        RenderQuad();
    }

    // 2. Blur the emissive areas with ping-pong
    Shader* blurShader = SHADER_MANAGER.GetShader(ShaderManager::BLOOM_BLUR);
    if (!blurShader) return;

    blurShader->use();
    for (unsigned int i = 0; i < blurIterations; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[1]);
        blurShader->setBool("horizontal", true);
        blurShader->setFloat("blurStrength", blurStrength);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[0]);
        RenderQuad();

        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        blurShader->setBool("horizontal", false);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[1]);
        RenderQuad();
    }

    // 3. Combine scene with bloom
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Shader* finalShader = SHADER_MANAGER.GetShader(ShaderManager::BLOOM_FINAL);
    if (finalShader) {
        finalShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // Main scene
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[0]); // Blurred emissive
        finalShader->setInt("scene", 0);
        finalShader->setInt("bloomBlur", 1);
        finalShader->setFloat("bloomIntensity", bloomIntensity);   

        RenderQuad();
    }
}

void BloomSystem::SetupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void BloomSystem::RenderQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}