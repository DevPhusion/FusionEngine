#include "../../../../Header Files/Core/Physics/Collision/DebugPoint.h"
#include "../../../../Header Files/Core/EngineManager.h" 
#include "../../../../Header Files/Core/Camera.h"        

void DebugPoint::Initialize() {
    float vertex[] = { 0.0f, 0.0f, 0.0f };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void DebugPoint::DrawPoint(glm::vec3 position, float pixelSize, Shader shader) {
    if (VAO == 0) {
        Initialize();
    }

    shader.use();

    glm::mat4 projection = glm::ortho(-EngineManager::getInstance().gameAspectRatio, EngineManager::getInstance().gameAspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    shader.setMat4D("projection", projection);
    shader.setMat4D("view", Camera::getInstance().viewMatrix);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);
    shader.setMat4D("transform", transform);

 
    shader.setVec4D("aColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    glPointSize(pixelSize);

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);

    glPointSize(1.0f);
}