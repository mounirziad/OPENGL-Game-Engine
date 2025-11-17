#pragma once  // Include guard - prevent multiple inclusions
#include <glm/glm.hpp>                      // GLM math library
#include <glm/gtc/matrix_transform.hpp>     // GLM matrix transformations
#include <vector>                           // Standard vector container

// Enum for camera movement directions
enum Camera_Movement {
    FORWARD,   // Move forward
    BACKWARD,  // Move backward  
    LEFT,      // Move left
    RIGHT      // Move right
};

// Default camera values - constants
const float YAW = -90.0f;         // Initial left/right rotation (faces -Z axis)
const float PITCH = 0.0f;         // Initial up/down rotation (level)
const float SPEED = 2.5f;         // Movement speed units per second
const float SENSITIVITY = 0.1f;   // Mouse sensitivity multiplier
const float ZOOM = 45.0f;         // Field of view in degrees

class Camera {
private:
    glm::mat4 projection;          // Projection matrix
    float aspectRatio;             // Current aspect ratio

public:
    // camera Attributes - vectors defining camera orientation
    glm::vec3 Position;    // Camera position in world space
    glm::vec3 Front;       // Direction camera is facing
    glm::vec3 Up;          // Camera's up direction
    glm::vec3 Right;       // Camera's right direction
    glm::vec3 WorldUp;     // World's up direction (usually (0,1,0))

    // euler Angles - rotation values
    float Yaw;     // Left/right rotation (around Y-axis)
    float Pitch;   // Up/down rotation (around X-axis)

    // camera options - adjustable settings
    float MovementSpeed;     // How fast camera moves
    float MouseSensitivity;  // How sensitive mouse movement is
    float Zoom;              // Field of view/zoom level

    // Constructor - initialize camera with default or provided values
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),  // Default position
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),           // Default up vector
        float yaw = YAW,                                      // Default yaw
        float pitch = PITCH) :                                // Default pitch
        Front(glm::vec3(0.0f, 0.0f, -1.0f)),                 // Initialize front vector
        MovementSpeed(SPEED),                                // Set movement speed
        MouseSensitivity(SENSITIVITY),                       // Set mouse sensitivity
        Zoom(ZOOM)                                           // Set zoom level
    {
        Position = position;  // Set camera position
        WorldUp = up;         // Set world up vector
        Yaw = yaw;           // Set yaw angle
        Pitch = pitch;       // Set pitch angle
        aspectRatio = 1.0f;  // Default aspect ratio
        projection = glm::mat4(1.0f); // Initialize projection matrix
        updateCameraVectors(); // Calculate front, right, up vectors from angles
    }

    // Update projection matrix with new aspect ratio
    void UpdateProjectionMatrix(float newAspectRatio) {
        aspectRatio = newAspectRatio;
        projection = glm::perspective(glm::radians(Zoom), aspectRatio, 0.1f, 100.0f);
    }

    // returns view matrix - creates lookAt matrix for shader
    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);  // Create view matrix: eye, center, up
    }

    // returns projection matrix
    glm::mat4 GetProjectionMatrix() const {
        return projection;
    }

    // Get current aspect ratio
    float GetAspectRatio() const {
        return aspectRatio;
    }

    // keyboard input - process keyboard movement
    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;  // Calculate movement distance for this frame
        if (direction == FORWARD)
            Position += Front * velocity;   // Move in front direction
        if (direction == BACKWARD)
            Position -= Front * velocity;   // Move opposite front direction
        if (direction == LEFT)
            Position -= Right * velocity;   // Move left (negative right direction)
        if (direction == RIGHT)
            Position += Right * velocity;   // Move right (positive right direction)
    }

    // mouse movement input - process mouse look
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= MouseSensitivity;  // Scale mouse movement by sensitivity
        yoffset *= MouseSensitivity;  // Scale mouse movement by sensitivity

        Yaw += xoffset;    // Add horizontal movement to yaw
        Pitch += yoffset;  // Add vertical movement to pitch

        // Prevent camera from flipping upside down
        if (constrainPitch) {
            if (Pitch > 89.0f) Pitch = 89.0f;   // Clamp looking too far up
            if (Pitch < -89.0f) Pitch = -89.0f; // Clamp looking too far down
        }

        updateCameraVectors();  // Recalculate direction vectors from new angles
    }

    // mouse scroll - process zoom/field of view change
    void ProcessMouseScroll(float yoffset) {
        Zoom -= yoffset;         // Adjust zoom level by scroll amount
        if (Zoom < 1.0f) Zoom = 1.0f;     // Clamp minimum zoom
        if (Zoom > 45.0f) Zoom = 45.0f;   // Clamp maximum zoom

        // Update projection matrix with new zoom
        projection = glm::perspective(glm::radians(Zoom), aspectRatio, 0.1f, 100.0f);
    }

private:
    // calculate front, right, up from yaw/pitch - convert angles to direction vectors
    void updateCameraVectors() {
        glm::vec3 front;  // Temporary front vector
        // Calculate new front vector using trigonometry
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));  // X component
        front.y = sin(glm::radians(Pitch));                           // Y component  
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));  // Z component
        Front = glm::normalize(front);                                // Normalize to unit length

        // Calculate right and up vectors using cross products
        Right = glm::normalize(glm::cross(Front, WorldUp));  // Right = front cross world up
        Up = glm::normalize(glm::cross(Right, Front));       // Up = right cross front
    }
};