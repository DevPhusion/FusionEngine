#include "../../../../Header Files/Core/Physics/Collision/DebugCircle.h"

void DebugCircle::Initialize() {
    std::vector<float> vertices;
    float increment = 2.0f * 3.14159265f / segments;

    for (int i = 0; i < segments; ++i) {
        float theta = i * increment;
        vertices.push_back(std::cos(theta)); 
        vertices.push_back(std::sin(theta)); 
        vertices.push_back(0.0f);            
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void DebugCircle::DrawCircle(glm::vec3 center, float radius, Shader shader) {
    if (VAO == 0) {
        Initialize();
    }

    shader.use();

    glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    shader.setMat4D("projection", projection);
    shader.setMat4D("view", Camera::getInstance().viewMatrix);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, center);
    transform = glm::scale(transform, glm::vec3(radius, radius, 1.0f));
    shader.setMat4D("transform", transform);

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINE_LOOP, 0, segments);
    glBindVertexArray(0);
}