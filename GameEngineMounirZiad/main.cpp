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
#include "PhysicsSystem.h"

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
Camera camera(glm::vec3(0.0f, 5.0f, 10.0f)); // Start higher to see falling objects

// Mouse and input variables
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
bool cursorEnabled = false;
double lastMouseX = SCR_WIDTH / 2.0;
double lastMouseY = SCR_HEIGHT / 2.0;

// timing - variables for smooth movement
float deltaTime = 0.0f;
float lastFrame = 0.0f;

static size_t gpuMemoryUsage = 0;
static size_t systemMemoryUsage = 0;

// Vector to track all cubes
std::vector<Entity> cubeEntities;
std::vector<Entity> wallEntities; // Track wall entities for cleanup

// Function to create a new cube entity
Entity CreateCube(Registry& registry, const glm::vec3& position, Material* material, unsigned int VAO) {
    Entity newCube = registry.CreateEntity();

    // Add transform component
    registry.AddComponent(newCube, TransformComponent(
        position,
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }
    ));

    // Add physics component
    registry.AddComponent(newCube, PhysicsComponent(true, 1.0f));

    // Add collider component
    registry.AddComponent(newCube, ColliderComponent(ColliderType::BOX));
    auto collider = registry.GetComponent<ColliderComponent>(newCube);
    if (collider) {
        collider->radius = 0.866f; // Approximate sphere radius for cube
    }

    // Add mesh component
    MeshComponent meshComp;
    meshComp.VAO = VAO;
    meshComp.vertexCount = 36;
    meshComp.material = material;
    registry.AddComponent(newCube, meshComp);

    return newCube;
}

// Temporary test function to see GI effects
void CreateGITestScene(Registry& registry, unsigned int VAO, ShaderManager& shaderManager) {
    std::cout << "Creating GI test scene with colored walls..." << std::endl;

    // Create colored walls around the scene to see color bleeding
    std::vector<glm::vec3> wallColors = {
        glm::vec3(1.0f, 0.0f, 0.0f), // Red
        glm::vec3(0.0f, 1.0f, 0.0f), // Green  
        glm::vec3(0.0f, 0.0f, 1.0f), // Blue
        glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow
    };

    std::vector<glm::vec3> wallPositions = {
        glm::vec3(-8.0f, 2.0f, 0.0f),
        glm::vec3(8.0f, 2.0f, 0.0f),
        glm::vec3(0.0f, 2.0f, -8.0f),
        glm::vec3(0.0f, 2.0f, 8.0f)
    };

    std::vector<glm::vec3> wallRotations = {
        glm::vec3(0.0f, 90.0f, 0.0f),  // Red wall - rotated to face inward
        glm::vec3(0.0f, -90.0f, 0.0f), // Green wall - rotated to face inward
        glm::vec3(0.0f, 0.0f, 0.0f),   // Blue wall - no rotation
        glm::vec3(0.0f, 180.0f, 0.0f)  // Yellow wall - rotated to face inward
    };

    for (int i = 0; i < 4; i++) {
        Material* wallMaterial = new Material(shaderManager.GetShader(ShaderManager::PHONG), wallColors[i]);
        wallMaterial->albedo = wallColors[i];
        wallMaterial->roughness = 0.8f; // Rough walls for better GI diffusion
        wallMaterial->emissive = 0.0f;  // EXPLICITLY NON-EMISSIVE - walls should not glow!

        Entity wall = registry.CreateEntity();
        registry.AddComponent(wall, TransformComponent(
            wallPositions[i],
            wallRotations[i],
            glm::vec3(0.5f, 4.0f, 6.0f) // Thinner, taller walls
        ));

        // Add physics but disable gravity so walls stay in place
        registry.AddComponent(wall, PhysicsComponent(false, 0.0f)); // No gravity, infinite mass
        registry.AddComponent(wall, ColliderComponent(ColliderType::BOX));

        MeshComponent wallMesh;
        wallMesh.VAO = VAO;
        wallMesh.vertexCount = 36;
        wallMesh.material = wallMaterial;
        registry.AddComponent(wall, wallMesh);

        wallEntities.push_back(wall);
        std::cout << "Created " << (i == 0 ? "Red" : i == 1 ? "Green" : i == 2 ? "Blue" : "Yellow") << " wall" << std::endl;
    }

    std::cout << "GI test scene created with " << wallEntities.size() << " colored walls" << std::endl;
}

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

// Enhanced Function to load a new model with physics
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
            { 5.0f, 8.0f, -8.0f },  // Start higher for falling
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f }
        ));

        // Add physics to model
        registry.AddComponent(newModel, PhysicsComponent(true, 2.0f)); // Heavier mass
        registry.AddComponent(newModel, ColliderComponent(ColliderType::SPHERE));

        MeshComponent loadedMeshComp;
        loadedMeshComp.VAO = loadedVAO;
        loadedMeshComp.vertexCount = loadedVertexCount;
        loadedMeshComp.material = material;
        registry.AddComponent(newModel, loadedMeshComp);

        // Update memory usage
        gpuMemUsage = sizeof(float) * loadedVertexCount * 8;
        sysMemUsage = sizeof(TransformComponent) + sizeof(MeshComponent) +
            sizeof(PhysicsComponent) + sizeof(ColliderComponent);

        currentModelEntity = newModel;
        std::cout << "Successfully loaded model with physics: " << modelPath << std::endl;
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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Game Engine with Physics, GI and Bloom", NULL, NULL);
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

    // Initialize camera projection with correct aspect ratio
    float initialAspectRatio = (float)SCR_WIDTH / (float)SCR_HEIGHT;
    camera.UpdateProjectionMatrix(initialAspectRatio);

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

    // Create systems
    TerrainSystem terrainSystem;
    PhysicsSystem physicsSystem;

    // Create terrain entity
    Entity terrainEntity = registry.CreateEntity();
    registry.AddComponent(terrainEntity, TransformComponent(
        { 0.0f, -2.0f, 0.0f },  // Position terrain lower
        { 0.0f, 0.0f, 0.0f },   // No rotation
        { 1.0f, 1.0f, 1.0f }    // Scale
    ));

    // Create and generate terrain
    TerrainComponent terrainComp(128, 128, 0.5f, 5.0f); // Larger, more detailed terrain with higher hills
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

    // Create cube entity with physics
    Entity cube = registry.CreateEntity();
    registry.AddComponent(cube, TransformComponent(
        { 0.0f, 10.0f, -5.0f },  // Start higher for falling
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }
    ));

    // Add physics to cube
    registry.AddComponent(cube, PhysicsComponent(true, 1.0f)); // Use gravity, mass = 1
    registry.AddComponent(cube, ColliderComponent(ColliderType::BOX));
    auto cubeCollider = registry.GetComponent<ColliderComponent>(cube);
    if (cubeCollider) {
        cubeCollider->radius = 0.866f; // Approximate sphere radius for cube (sqrt(3)/2)
    }

    // Create materials for CUBE using ShaderManager
    Material* cubePhongMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PHONG), glm::vec3(1.0f, 0.0f, 0.0f));
    Material* cubePbrMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(0.0f, 1.0f, 0.0f));
    Material* cubeWireframeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::WIREFRAME), glm::vec3(0.0f, 0.0f, 1.0f));
    Material* cubeFlatMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::FLAT), glm::vec3(1.0f, 1.0f, 0.0f));
    Material* cubeUnlitMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::UNLIT), glm::vec3(1.0f, 0.0f, 1.0f));

    // Create materials for LOADED MODEL using ShaderManager  
    Material* modelPhongMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PHONG), glm::vec3(0.8f, 0.2f, 0.1f)); // Red car
    Material* modelPbrMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(0.8f, 0.2f, 0.1f)); // Red car
    Material* modelWireframeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::WIREFRAME), glm::vec3(0.3f, 0.3f, 0.8f));
    Material* modelFlatMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::FLAT), glm::vec3(0.8f, 0.8f, 0.3f));
    Material* modelUnlitMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::UNLIT), glm::vec3(0.8f, 0.3f, 0.8f));

    // Ensure ALL materials are explicitly non-emissive by default
    cubePhongMaterial->emissive = 0.0f;
    cubePbrMaterial->emissive = 0.0f;  // Regular PBR should also be non-emissive
    cubeWireframeMaterial->emissive = 0.0f;
    cubeFlatMaterial->emissive = 0.0f;
    cubeUnlitMaterial->emissive = 0.0f;

    modelPhongMaterial->emissive = 0.0f;
    modelPbrMaterial->emissive = 0.0f;  // Model PBR should also be non-emissive
    modelWireframeMaterial->emissive = 0.0f;
    modelFlatMaterial->emissive = 0.0f;
    modelUnlitMaterial->emissive = 0.0f;

    // Enable wireframe for wireframe materials
    cubeWireframeMaterial->SetWireframe(true);
    modelWireframeMaterial->SetWireframe(true);

    // FIXED: Make regular PBR material NON-EMISSIVE
    cubePbrMaterial->SetEmissive(0.0f, glm::vec3(0.0f, 0.0f, 0.0f)); // NO emissive for regular material
    modelPbrMaterial->SetEmissive(0.0f, glm::vec3(0.0f, 0.0f, 0.0f)); // NO emissive for model material

    // Create a special glowing material for bloom testing with MUCH higher intensity
    Material* glowingCubeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(1.0f, 0.8f, 0.2f));
    glowingCubeMaterial->SetEmissive(15.0f, glm::vec3(1.0f, 0.8f, 0.2f)); // VERY HIGH emissive for strong glow
    glowingCubeMaterial->albedo = glm::vec3(1.0f, 0.8f, 0.2f);
    glowingCubeMaterial->roughness = 0.1f; // Smooth surface
    glowingCubeMaterial->metallic = 0.0f;  // Non-metallic for better glow

    // Set PBR properties for better GI response
    cubePbrMaterial->roughness = 0.3f;
    cubePbrMaterial->metallic = 0.1f;
    cubePbrMaterial->albedo = cubePbrMaterial->color; // Use material color

    modelPbrMaterial->roughness = 0.4f;
    modelPbrMaterial->metallic = 0.8f; // More metallic for car
    modelPbrMaterial->albedo = modelPbrMaterial->color; // Use material color

    // Give cube a mesh component with default material
    MeshComponent meshComp;
    meshComp.VAO = VAO;
    meshComp.vertexCount = 36;
    meshComp.material = cubePhongMaterial; // Start with cube phong material
    registry.AddComponent(cube, meshComp);

    // Add the first cube to our tracking vector
    cubeEntities.push_back(cube);

    // Create test scene to see GI effects
    CreateGITestScene(registry, VAO, SHADER_MANAGER);

    // Load initial model
    if (!availableModels.empty()) {
        LoadNewModel(registry, availableModels[currentModelIndex], loadedModelEntity,
            modelPbrMaterial, gpuMemoryUsage, systemMemoryUsage);
    }

    // Create and initialize render system with GI and Bloom
    RenderSystem renderer;
    if (!renderer.Initialize(SCR_WIDTH, SCR_HEIGHT)) {
        std::cout << "Warning: Render system initialization had issues" << std::endl;
    }

    // Lighting variables 
    glm::vec3 lightPos(1.2f, 8.0f, 2.0f); // Higher position for better shadows
    glm::vec3 lightColor(1.2f, 1.1f, 1.0f); // Warmer light color

    // UI state variables
    static int currentShader = 0;
    static int currentMaterial = 0;
    static int previousModelIndex = -1;  // Track model changes
    static float terrainHeightScale = 5.0f;
    static float terrainScale = 0.5f;
    static bool giEnabled = true;
    static bool shadowsEnabled = false; // Start with shadows disabled for GI testing
    static float giIntensity = 1.0f;
    static bool showTestScene = true; // Control for test scene

    // Bloom UI state variables - LOWER THRESHOLD and HIGHER INTENSITY
    static bool bloomEnabled = true; // Start with bloom enabled
    static float bloomThreshold = 1.8f; // MUCH LOWER threshold to catch emissive objects
    static float bloomIntensity = 1.5f; // Higher intensity
    static int blurIterations = 8;
    static float blurStrength = 1.5f;

    // Material type for cube (Regular vs Glowing)
    static int materialType = 0; // 0 = Regular, 1 = Glowing

    // Debug bloom counter
    static int framesWithBloom = 0;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Timing
        float currentFrame = static_cast<float>(glfwGetTime());
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

        // Update physics BEFORE rendering
        physicsSystem.Update(registry, deltaTime);

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

        // GI status info
        ImGui::Text("GI: %s | Shadows: %s | Bloom: %s",
            giEnabled ? "Active" : "Inactive",
            shadowsEnabled ? "Active" : "Inactive",
            bloomEnabled ? "Active" : "Inactive");

        // Light Controls
        ImGui::ColorEdit3("Light Color", (float*)&lightColor);
        ImGui::SliderFloat3("Light Position", (float*)&lightPos, -10.0f, 10.0f);

        // Physics Controls
        ImGui::Separator();
        ImGui::Text("Physics Controls");

        if (auto physics = registry.GetComponent<PhysicsComponent>(cube)) {
            ImGui::Checkbox("Cube Use Gravity", &physics->useGravity);
            ImGui::SliderFloat("Cube Mass", &physics->mass, 0.1f, 10.0f);
            ImGui::SliderFloat("Bounciness", &physics->restitution, 0.0f, 1.0f);
            ImGui::Text("Cube Grounded: %s", physics->isGrounded ? "Yes" : "No");

            if (ImGui::Button("Reset Cube Position")) {
                if (auto transform = registry.GetComponent<TransformComponent>(cube)) {
                    transform->position = { 0.0f, 10.0f, -5.0f };
                    physics->velocity = { 0.0f, 0.0f, 0.0f };
                    physics->isGrounded = false;
                }
            }
        }

        // Model physics controls
        if (auto physics = registry.GetComponent<PhysicsComponent>(loadedModelEntity)) {
            ImGui::Checkbox("Model Use Gravity", &physics->useGravity);
            ImGui::Text("Model Grounded: %s", physics->isGrounded ? "Yes" : "No");

            if (ImGui::Button("Reset Model Position")) {
                if (auto transform = registry.GetComponent<TransformComponent>(loadedModelEntity)) {
                    transform->position = { 2.0f, 8.0f, -5.0f };
                    physics->velocity = { 0.0f, 0.0f, 0.0f };
                    physics->isGrounded = false;
                }
            }
        }

        // Cube Management
        ImGui::Separator();
        ImGui::Text("Cube Management");

        // Button to add new cube 
        if (ImGui::Button("Add New Cube")) {
            // Create a random position around the scene
            float x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f - 5.0f;
            float z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f - 5.0f;

            // Create a random color
            float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

            // Create a new material with random color and GI properties
            Material* newCubeMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PHONG), glm::vec3(r, g, b));
            newCubeMaterial->albedo = glm::vec3(r, g, b);
            newCubeMaterial->roughness = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            newCubeMaterial->metallic = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.5f;
            newCubeMaterial->emissive = 0.0f; // EXPLICITLY NON-EMISSIVE by default

            // Make some cubes emissive for bloom (but only 10% chance now)
            if (rand() % 10 == 0) { // 10% chance to be emissive
                float emissiveIntensity = 3.0f + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 5.0f;
                newCubeMaterial->SetEmissive(emissiveIntensity, glm::vec3(r, g, b));
                std::cout << "Created glowing cube with intensity: " << emissiveIntensity << std::endl;
            }

            // Create the new cube
            Entity newCube = CreateCube(registry,
                glm::vec3(x, 15.0f, z), // Start high so it falls
                newCubeMaterial,
                VAO);

            // Add to tracking vector
            cubeEntities.push_back(newCube);

            std::cout << "Added new cube at position: " << x << ", 15.0, " << z << std::endl;
        }

        // Display cube count
        ImGui::Text("Total Cubes: %zu", cubeEntities.size());

        // Button to remove all cubes except the first one
        if (ImGui::Button("Remove All Extra Cubes") && cubeEntities.size() > 1) {
            // Remove all cubes except the first one
            for (size_t i = 1; i < cubeEntities.size(); i++) {
                registry.DestroyEntity(cubeEntities[i]);
            }
            cubeEntities.resize(1); // Keep only the first cube
            std::cout << "Removed all extra cubes" << std::endl;
        }

        // Button to reset all cubes positions
        if (ImGui::Button("Reset All Cubes")) {
            for (size_t i = 0; i < cubeEntities.size(); i++) {
                if (auto transform = registry.GetComponent<TransformComponent>(cubeEntities[i])) {
                    if (auto physics = registry.GetComponent<PhysicsComponent>(cubeEntities[i])) {
                        // Random position for variety
                        float x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f - 5.0f;
                        float z = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 10.0f - 5.0f;

                        transform->position = glm::vec3(x, 10.0f + i * 2.0f, z); // Stagger heights
                        physics->velocity = { 0.0f, 0.0f, 0.0f };
                        physics->isGrounded = false;
                    }
                }
            }
            std::cout << "Reset all cube positions" << std::endl;
        }

        // Individual cube controls (optional - for more advanced management)
        if (cubeEntities.size() > 0) {
            ImGui::Separator();
            ImGui::Text("Cube %d Controls:", 0);
            if (auto t = registry.GetComponent<TransformComponent>(cubeEntities[0])) {
                ImGui::SliderFloat3("Cube 0 Position", (float*)&t->position, -10.0f, 10.0f);
            }
        }

        // Cube Transform
        ImGui::Separator();
        ImGui::Text("Cube Settings");
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
        ImGui::Text("Cube Shader Settings");
        const char* shaders[] = {
            "Phong", "PBR", "Wireframe", "Flat", "Unlit",
            "Depth", "GI Apply", "Voxelization"
        };
        ImGui::Combo("Shader Type", &currentShader, shaders, 8);

        // Material type selection (Regular vs Glowing) - ONLY FOR PBR SHADER
        ImGui::Separator();
        ImGui::Text("Material Type");
        if (currentShader == 1) { // Only show for PBR shader
            if (ImGui::Combo("Material Type", &materialType, "Regular\0Glowing\0")) {
                if (auto mesh = registry.GetComponent<MeshComponent>(cube)) {
                    if (materialType == 0) {
                        // Regular PBR material - NON-EMISSIVE
                        mesh->material = cubePbrMaterial;
                        std::cout << "Switched to REGULAR material (non-emissive)" << std::endl;
                    }
                    else {
                        // Glowing material for bloom - HIGHLY EMISSIVE
                        mesh->material = glowingCubeMaterial;
                        std::cout << "Switched to GLOWING material (highly emissive)" << std::endl;
                    }
                }
            }
        }
        else {
            // For non-PBR shaders, hide material type and reset to regular
            materialType = 0;
            ImGui::Text("Material Type: Regular (PBR only)");
        }

        // Material color selection
        const char* materials[] = { "Red", "Green", "Blue", "Yellow", "Magenta" };
        ImGui::Combo("Material Color", &currentMaterial, materials, 5);

        // Terrain Controls
        ImGui::Separator();
        ImGui::Text("Terrain Controls");

        if (auto t = registry.GetComponent<TerrainComponent>(terrainEntity)) {
            ImGui::Checkbox("Wireframe", &t->wireframe);

            if (ImGui::SliderFloat("Height Scale", &terrainHeightScale, 0.1f, 10.0f)) {
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

        // Global Illumination Controls
        ImGui::Separator();
        ImGui::Text("Global Illumination");
        ImGui::Checkbox("Enable GI", &giEnabled);
        ImGui::Checkbox("Enable Shadows", &shadowsEnabled);
        ImGui::SliderFloat("GI Intensity", &giIntensity, 0.0f, 2.0f);

        // Bloom Effects Controls
        ImGui::Separator();
        ImGui::Text("Bloom Effects");
        ImGui::Checkbox("Enable Bloom", &bloomEnabled);
        ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.5f, 3.0f);
        ImGui::Text("(Higher = only very bright objects glow)");
        ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 3.0f);
        ImGui::SliderInt("Blur Iterations", &blurIterations, 1, 20);
        ImGui::SliderFloat("Blur Strength", &blurStrength, 0.1f, 3.0f);

        // Also add a test button to create a guaranteed glowing cube:
        if (ImGui::Button("Add Test Glowing Cube")) {
            Material* testGlowingMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::PBR), glm::vec3(0.2f, 0.8f, 1.0f));
            testGlowingMaterial->SetEmissive(20.0f, glm::vec3(0.2f, 0.8f, 1.0f)); // Very high blue emissive
            testGlowingMaterial->albedo = glm::vec3(0.2f, 0.8f, 1.0f);
            testGlowingMaterial->roughness = 0.1f;
            testGlowingMaterial->metallic = 0.0f;

            Entity testCube = CreateCube(registry,
                glm::vec3(0.0f, 15.0f, 0.0f),
                testGlowingMaterial,
                VAO);

            cubeEntities.push_back(testCube);
            std::cout << "Added TEST glowing cube with high emissive intensity!" << std::endl;
        }

        // Test scene controls
        ImGui::Checkbox("Show GI Test Scene", &showTestScene);
        if (ImGui::Button("Toggle Test Scene")) {
            showTestScene = !showTestScene;
            // Toggle wall visibility by enabling/disabling rendering
            for (auto wall : wallEntities) {
                if (auto mesh = registry.GetComponent<MeshComponent>(wall)) {
                    // Store the original material and swap with a transparent one
                    static std::unordered_map<Entity, Material*> originalMaterials;

                    if (showTestScene) {
                        // Restore original material if it was stored
                        if (originalMaterials.find(wall) != originalMaterials.end()) {
                            mesh->material = originalMaterials[wall];
                            originalMaterials.erase(wall);
                        }
                    }
                    else {
                        // Store original material and set to null to hide
                        if (mesh->material) {
                            originalMaterials[wall] = mesh->material;
                            mesh->material = nullptr; // This will prevent rendering
                        }
                    }
                }
            }
            std::cout << "Test scene " << (showTestScene ? "shown" : "hidden") << std::endl;
        }

        // Apply settings to render system
        renderer.SetGIEnabled(giEnabled);
        renderer.SetShadowsEnabled(shadowsEnabled);
        renderer.SetBloomEnabled(bloomEnabled);
        renderer.SetBloomIntensity(bloomIntensity);
        renderer.SetBloomThreshold(bloomThreshold);
        renderer.SetBlurStrength(blurStrength);
        renderer.SetBlurIterations(blurIterations);

        ImGui::End();

        // Apply selected shader and material to cube ONLY
        if (auto mesh = registry.GetComponent<MeshComponent>(cube)) {

            // Store previous material for cleanup
            static Material* previousSpecialMaterial = nullptr;

            // Clean up previous special material if it exists
            if (previousSpecialMaterial &&
                previousSpecialMaterial != cubePhongMaterial &&
                previousSpecialMaterial != cubePbrMaterial &&
                previousSpecialMaterial != cubeWireframeMaterial &&
                previousSpecialMaterial != cubeFlatMaterial &&
                previousSpecialMaterial != cubeUnlitMaterial &&
                previousSpecialMaterial != glowingCubeMaterial) {
                delete previousSpecialMaterial;
                previousSpecialMaterial = nullptr;
            }

            // Update shader for CUBE only
            switch (currentShader) {
            case 0: mesh->material = cubePhongMaterial; break;
            case 1:
                // For PBR shader, respect the material type selection
                if (materialType == 0) {
                    mesh->material = cubePbrMaterial;
                }
                else {
                    mesh->material = glowingCubeMaterial;
                }
                break;
            case 2: mesh->material = cubeWireframeMaterial; break;
            case 3: mesh->material = cubeFlatMaterial; break;
            case 4: mesh->material = cubeUnlitMaterial; break;
            case 5: // Depth
                previousSpecialMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::DEPTH), glm::vec3(0.5f, 0.5f, 1.0f));
                previousSpecialMaterial->albedo = glm::vec3(0.5f, 0.5f, 1.0f);
                previousSpecialMaterial->emissive = 0.0f; // Non-emissive
                mesh->material = previousSpecialMaterial;
                break;
            case 6: // GI Apply
                previousSpecialMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::GI_APPLY), glm::vec3(0.8f, 0.8f, 0.8f));
                previousSpecialMaterial->albedo = glm::vec3(0.8f, 0.8f, 0.8f);
                previousSpecialMaterial->roughness = 0.5f;
                previousSpecialMaterial->metallic = 0.2f;
                previousSpecialMaterial->emissive = 0.0f; // Non-emissive
                mesh->material = previousSpecialMaterial;
                break;
            case 7: // Voxelization
                previousSpecialMaterial = new Material(SHADER_MANAGER.GetShader(ShaderManager::VOXELIZATION), glm::vec3(0.9f, 0.2f, 0.2f));
                previousSpecialMaterial->albedo = glm::vec3(0.9f, 0.2f, 0.2f);
                previousSpecialMaterial->emissive = 0.0f; // Non-emissive
                mesh->material = previousSpecialMaterial;
                break;
            }

            // Only update color for the first 5 shaders (your existing materials)
            if (currentShader < 5) {
                // Update material color and GI properties for CUBE only
                glm::vec3 newColor;
                switch (currentMaterial) {
                case 0: newColor = glm::vec3(1.0f, 0.0f, 0.0f); break; // Red
                case 1: newColor = glm::vec3(0.0f, 1.0f, 0.0f); break; // Green
                case 2: newColor = glm::vec3(0.0f, 0.0f, 1.0f); break; // Blue
                case 3: newColor = glm::vec3(1.0f, 1.0f, 0.0f); break; // Yellow
                case 4: newColor = glm::vec3(1.0f, 0.0f, 1.0f); break; // Magenta
                default: newColor = glm::vec3(1.0f, 1.0f, 1.0f); break;
                }

                mesh->material->color = newColor;
                mesh->material->albedo = newColor;

                // Reset emissive when changing colors (except for glowing material)
                if (materialType == 0 && currentShader == 1) { // Only for PBR regular material
                    mesh->material->emissive = 0.0f;
                }
            }
        }

        // Debug bloom output
        if (bloomEnabled) {
            framesWithBloom++;
            if (framesWithBloom % 60 == 0) { // Every second
                std::cout << "Bloom active - Threshold: " << bloomThreshold
                    << " Intensity: " << bloomIntensity << std::endl;
                if (materialType == 1) {
                    std::cout << "Glowing cube should be visible with bloom!" << std::endl;
                }
            }
        }

        // Render terrain first (as background)
        terrainSystem.RenderTerrain(registry, camera, SCR_WIDTH, SCR_HEIGHT, lightPos, lightColor);

        // Then render other objects using ECS with bloom pipeline
        renderer.Render(registry, camera, SCR_WIDTH, SCR_HEIGHT, lightPos, lightColor);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Animate light position for dynamic GI testing
        lightPos.x = static_cast<float>(sin(glfwGetTime() * 0.5f) * 3.0f);
        lightPos.y = 8.0f + static_cast<float>(cos(glfwGetTime() * 0.3f) * 1.0f);
        lightPos.z = static_cast<float>(cos(glfwGetTime() * 0.4f) * 3.0f);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    if (auto terrain = registry.GetComponent<TerrainComponent>(terrainEntity)) {
        terrainSystem.CleanupTerrain(*terrain);
    }
    renderer.Cleanup();

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
    delete glowingCubeMaterial;

    // Clean up any additional cube materials
    for (size_t i = 1; i < cubeEntities.size(); i++) {
        if (auto mesh = registry.GetComponent<MeshComponent>(cubeEntities[i])) {
            delete mesh->material;
        }
    }

    // Clean up wall materials
    for (auto wall : wallEntities) {
        if (auto mesh = registry.GetComponent<MeshComponent>(wall)) {
            delete mesh->material;
        }
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

// Updated framebuffer_size_callback to handle camera scaling
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    // Update camera projection matrix with new aspect ratio
    float aspectRatio = (float)width / (float)height;
    camera.UpdateProjectionMatrix(aspectRatio);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (cursorEnabled) {
        // When cursor is enabled (UI mode), don't process camera movement
        lastX = xpos;
        lastY = ypos;
        firstMouse = true; // Reset for next time we disable cursor
        return;
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void processInput(GLFWwindow* window) {
    static bool tabPressedLastFrame = false;

    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabPressedLastFrame) {
        cursorEnabled = !cursorEnabled;

        if (cursorEnabled) {
            // Enable cursor for UI
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            // Store current mouse position when entering UI mode
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        }
        else {
            // Disable cursor for camera control
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            // Restore mouse position to where it was when we entered UI mode
            glfwSetCursorPos(window, lastMouseX, lastMouseY);
            // Reset firstMouse so we don't get a jump
            firstMouse = true;
        }
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