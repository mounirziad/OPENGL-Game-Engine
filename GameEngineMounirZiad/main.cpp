// Include necessary headers for OpenGL, window creation, and math
#include <glad/glad.h>      // GLAD loads OpenGL function pointers
#include <GLFW/glfw3.h>     // GLFW handles window creation and input
#include <iostream>         // For console output
#include <vector>           // For model list
#include <filesystem>       // For scanning model directory

#include "Camera.h"         // Custom camera class for 3D navigation
#include <glm/glm.hpp>      // GLM math library for 3D mathematics
#include <glm/gtc/matrix_transform.hpp> // GLM matrix transformations

// ECS includes
#include "Registry.h"
#include "Component.h"
#include "RenderSystem.h"
#include "ModelLoader.h"
#include "TerrainSystem.h"
#include "TerrainComponent.h"

// New systems includes
#include "ShaderManager.h"
#include "TextureLoader.h"

// UI includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// Forward Declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings - window dimensions
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera - create camera instance and initialize mouse tracking variables
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// timing - variables for smooth movement
float deltaTime = 0.0f;
float lastFrame = 0.0f;

static size_t gpuMemoryUsage = 0;
static size_t systemMemoryUsage = 0;

// Function to scan models directory and get available models
std::vector<std::string> GetAvailableModels() {
    std::vector<std::string> models;

    // Common model extensions
    std::vector<std::string> extensions = { ".obj", ".fbx", ".gltf", ".glb" };

    try {
        // Check if models directory exists
        if (std::filesystem::exists("models")) {
            for (const auto& entry : std::filesystem::directory_iterator("models")) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();
                    // Convert to lowercase for comparison
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    // Check if file has a supported extension
                    for (const auto& ext : extensions) {
                        if (extension == ext) {
                            models.push_back(entry.path().filename().string());
                            break;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error scanning models directory: " << e.what() << std::endl;
    }

    // If no models found, add some default options
    if (models.empty()) {
        models = {
            "cube.obj",
            "sphere.obj",
            "teapot.obj",
            "Datsun_280Z.obj"
        };
    }

    return models;
}

// Function to load a new model
bool LoadNewModel(Registry& registry, const std::string& modelPath,
    Entity& currentModelEntity, Material* material,
    size_t& gpuMemUsage, size_t& sysMemUsage) {

    // Remove existing model if it exists
    if (currentModelEntity != -1) {
        registry.DestroyEntity(currentModelEntity);
        currentModelEntity = -1;
    }

    unsigned int loadedVAO;
    unsigned int loadedVertexCount;

    std::string fullPath = "models/" + modelPath;

    if (ModelLoader::LoadOBJ(fullPath, loadedVAO, loadedVertexCount)) {
        Entity newModel = registry.CreateEntity();
        registry.AddComponent(newModel, TransformComponent(
            { 2.0f, 0.0f, -5.0f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f }
        ));

        MeshComponent loadedMeshComp;
        loadedMeshComp.VAO = loadedVAO;
        loadedMeshComp.vertexCount = loadedVertexCount;
        loadedMeshComp.material = material;
        registry.AddComponent(newModel, loadedMeshComp);

        // Update memory usage (simplified calculation)
        gpuMemUsage = sizeof(float) * loadedVertexCount * 8;  // 3 pos + 3 normal + 2 UV approx
        sysMemUsage = sizeof(TransformComponent) + sizeof(MeshComponent);

        currentModelEntity = newModel;
        std::cout << "Successfully loaded model: " << modelPath << std::endl;
        return true;
    }
    else {
        std::cout << "Failed to load model: " << modelPath << std::endl;
        return false;
    }
}

int main()
{
    // Initialize GLFW
    glfwInit();
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }

    // Configure glfw hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create Window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Game Engine Example", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Setup callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Capture the mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Initialize new systems
    SHADER_MANAGER.LoadShaders();

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Set viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // ECS SETUP
    Registry registry;

    // Create terrain system
    TerrainSystem terrainSystem;

    // Create terrain entity
    Entity terrainEntity = registry.CreateEntity();
    registry.AddComponent(terrainEntity, TransformComponent(
        { 0.0f, -2.0f, 0.0f },  // Position terrain lower
        { 0.0f, 0.0f, 0.0f },   // No rotation
        { 1.0f, 1.0f, 1.0f }    // Scale
    ));

    // Create and generate terrain
    TerrainComponent terrainComp(128, 128, 0.5f, 2.0f); // Larger, more detailed terrain
    terrainSystem.GenerateTerrain(terrainComp);
    registry.AddComponent(terrainEntity, terrainComp);

    // Get available models
    std::vector<std::string> availableModels = GetAvailableModels();
    static int currentModelIndex = 0;
    static Entity loadedModelEntity = -1;  // Track the currently loaded model entity

    // Create cube entity with vertex data
    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create cube entity
    Entity cube = registry.CreateEntity();
    registry.AddComponent(cube, TransformComponent(
        { 0.0f, 1.0f, -5.0f },  // Position cube above terrain
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }
    ));

    // Create materials for CUBE using ShaderManager
    Material* cubePhongMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PHONG), glm::vec3(1.0f, 0.0f, 0.0f));
    Material* cubePbrMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(0.0f, 1.0f, 0.0f));
    Material* cubeWireframeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::WIREFRAME), glm::vec3(0.0f, 0.0f, 1.0f));
    Material* cubeFlatMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::FLAT), glm::vec3(1.0f, 1.0f, 0.0f));
    Material* cubeUnlitMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::UNLIT), glm::vec3(1.0f, 0.0f, 1.0f));

    // Create materials for LOADED MODEL using ShaderManager  
    Material* modelPhongMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PHONG), glm::vec3(0.5f, 0.5f, 0.5f));
    Material* modelPbrMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(0.7f, 0.7f, 0.7f));
    Material* modelWireframeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::WIREFRAME), glm::vec3(0.3f, 0.3f, 0.8f));
    Material* modelFlatMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::FLAT), glm::vec3(0.8f, 0.8f, 0.3f));
    Material* modelUnlitMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::UNLIT), glm::vec3(0.8f, 0.3f, 0.8f));

    // Enable wireframe for wireframe materials
    cubeWireframeMaterial->SetWireframe(true);
    modelWireframeMaterial->SetWireframe(true);

    // Give cube a mesh component with default material
    MeshComponent meshComp;
    meshComp.VAO = VAO;
    meshComp.vertexCount = 36;
    meshComp.material = cubePhongMaterial; // Start with cube phong material
    registry.AddComponent(cube, meshComp);

    // Load initial model
    if (!availableModels.empty()) {
        LoadNewModel(registry, availableModels[currentModelIndex], loadedModelEntity,
            modelPbrMaterial, gpuMemoryUsage, systemMemoryUsage);
    }

    // Create render system
    RenderSystem renderer;

    // Lighting variables
    glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

    // UI state variables - add terrain controls
    static int currentShader = 0;
    static int currentMaterial = 0;
    static int previousModelIndex = -1;  // Track model changes
    static float terrainHeightScale = 2.0f;
    static float terrainScale = 0.5f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Timing
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        static float fps = 0.0f;
        static float frameTimeMs = 0.0f;
        fps = 1.0f / deltaTime;
        frameTimeMs = deltaTime * 1000.0f;

        // Input
        processInput(window);

        // Clear screen
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Sky blue background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui Debug Window
        ImGui::Begin("Debug Controls");

        ImGui::Text("FPS: %.1f (%.2f ms/frame)", fps, frameTimeMs);
        ImGui::Separator();
        ImGui::Text("GPU Memory: %.2f KB", gpuMemoryUsage / 1024.0f);
        ImGui::Text("System Memory: %.2f KB", systemMemoryUsage / 1024.0f);

        // Light Controls
        ImGui::ColorEdit3("Light Color", (float*)&lightColor);
        ImGui::SliderFloat3("Light Position", (float*)&lightPos, -10.0f, 10.0f);

        // Cube Transform
        if (auto t = registry.GetComponent<TransformComponent>(cube)) {
            ImGui::SliderFloat3("Cube Position", (float*)&t->position, -10.0f, 10.0f);
            ImGui::SliderFloat3("Cube Rotation", (float*)&t->rotation, -180.0f, 180.0f);
            ImGui::SliderFloat3("Cube Scale", (float*)&t->scale, 0.1f, 5.0f);
        }

        // Model Selection
        ImGui::Separator();
        ImGui::Text("Model Selection");

        // Convert vector to array for ImGui
        std::vector<const char*> modelNames;
        for (const auto& model : availableModels) {
            modelNames.push_back(model.c_str());
        }

        if (!modelNames.empty()) {
            ImGui::Combo("Select Model", &currentModelIndex, modelNames.data(), (int)modelNames.size());

            // Load new model if selection changed
            if (currentModelIndex != previousModelIndex) {
                LoadNewModel(registry, availableModels[currentModelIndex], loadedModelEntity,
                    modelPbrMaterial, gpuMemoryUsage, systemMemoryUsage);
                previousModelIndex = currentModelIndex;
            }

            // Model transform controls
            if (auto t = registry.GetComponent<TransformComponent>(loadedModelEntity)) {
                ImGui::SliderFloat3("Model Position", (float*)&t->position, -10.0f, 10.0f);
                ImGui::SliderFloat3("Model Rotation", (float*)&t->rotation, -180.0f, 180.0f);
                ImGui::SliderFloat3("Model Scale", (float*)&t->scale, 0.1f, 5.0f);
            }
        }
        else {
            ImGui::Text("No models found in 'models' directory");
        }

        // Shader selection
        ImGui::Separator();
        ImGui::Text("Cube Settings");
        const char* shaders[] = { "Phong", "PBR", "Wireframe", "Flat", "Unlit" };
        ImGui::Combo("Shader Type", &currentShader, shaders, 5);

        // Material color selection
        const char* materials[] = { "Red", "Green", "Blue", "Yellow", "Magenta" };
        ImGui::Combo("Material Color", &currentMaterial, materials, 5);

        // Terrain Controls
        ImGui::Separator();
        ImGui::Text("Terrain Controls");

        if (auto t = registry.GetComponent<TerrainComponent>(terrainEntity)) {
            ImGui::Checkbox("Wireframe", &t->wireframe);

            if (ImGui::SliderFloat("Height Scale", &terrainHeightScale, 0.1f, 5.0f)) {
                t->heightScale = terrainHeightScale;
                terrainSystem.GenerateTerrain(*t); // Regenerate with new height scale
            }

            if (ImGui::SliderFloat("Terrain Scale", &terrainScale, 0.1f, 2.0f)) {
                t->scale = terrainScale;
                terrainSystem.GenerateTerrain(*t); // Regenerate with new scale
            }

            if (ImGui::Button("Regenerate Terrain")) {
                terrainSystem.GenerateTerrain(*t);
            }
        }

        // Transform controls for terrain
        if (auto transform = registry.GetComponent<TransformComponent>(terrainEntity)) {
            ImGui::SliderFloat3("Terrain Position", (float*)&transform->position, -10.0f, 10.0f);
        }

        ImGui::End();

        // Apply selected shader and material to cube ONLY
        if (auto mesh = registry.GetComponent<MeshComponent>(cube)) {
            // Update shader for CUBE only
            switch (currentShader) {
            case 0: mesh->material = cubePhongMaterial; break;
            case 1: mesh->material = cubePbrMaterial; break;
            case 2: mesh->material = cubeWireframeMaterial; break;
            case 3: mesh->material = cubeFlatMaterial; break;
            case 4: mesh->material = cubeUnlitMaterial; break;
            }

            // Update material color for CUBE only
            switch (currentMaterial) {
            case 0: mesh->material->color = glm::vec3(1.0f, 0.0f, 0.0f); break; // Red
            case 1: mesh->material->color = glm::vec3(0.0f, 1.0f, 0.0f); break; // Green
            case 2: mesh->material->color = glm::vec3(0.0f, 0.0f, 1.0f); break; // Blue
            case 3: mesh->material->color = glm::vec3(1.0f, 1.0f, 0.0f); break; // Yellow
            case 4: mesh->material->color = glm::vec3(1.0f, 0.0f, 1.0f); break; // Magenta
            }
        }

        // Render terrain first (as background)
        terrainSystem.RenderTerrain(registry, camera, SCR_WIDTH, SCR_HEIGHT, lightPos, lightColor);

        // Then render other objects using ECS
        renderer.Render(registry, camera, SCR_WIDTH, SCR_HEIGHT, lightPos, lightColor);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update ECS components
        auto transform = registry.GetComponent<TransformComponent>(cube);
        if (transform) {
            transform->rotation.y = (float)glfwGetTime() * 50.0f;
        }

        // Animate light position
        lightPos.x = sin(glfwGetTime()) * 2.0f;
        lightPos.y = cos(glfwGetTime()) * 1.0f;
        lightPos.z = cos(glfwGetTime() * 0.5f) * 2.0f;

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup - add terrain cleanup
    if (auto terrain = registry.GetComponent<TerrainComponent>(terrainEntity)) {
        terrainSystem.CleanupTerrain(*terrain);
    }

    // Clean up ALL materials
    delete cubePhongMaterial;
    delete cubePbrMaterial;
    delete cubeWireframeMaterial;
    delete cubeFlatMaterial;
    delete cubeUnlitMaterial;
    delete modelPhongMaterial;
    delete modelPbrMaterial;
    delete modelWireframeMaterial;
    delete modelFlatMaterial;
    delete modelUnlitMaterial;

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

// Existing callback functions remain the same...
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = (float)xposIn;
    float ypos = (float)yposIn;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll((float)yoffset);
}

bool cursorEnabled = false;

void processInput(GLFWwindow* window) {
    static bool tabPressedLastFrame = false;

    if (cursorEnabled) firstMouse = true;

    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabPressedLastFrame) {
        cursorEnabled = !cursorEnabled;
        glfwSetInputMode(window, GLFW_CURSOR, cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
    tabPressedLastFrame = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (!cursorEnabled) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
    }
}