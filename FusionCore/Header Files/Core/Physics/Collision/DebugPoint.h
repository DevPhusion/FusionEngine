#pragma once
#include <glad/glad.h> // Or your specific OpenGL/Glew header
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../../Rendering/Shader.h"

class DebugPoint {
private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    void Initialize();

public:
    void DrawPoint(glm::vec3 position, float radius, Shader shader);
};