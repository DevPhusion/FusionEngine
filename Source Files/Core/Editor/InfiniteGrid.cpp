#include "../../../Header Files/Core/Editor/InfiniteGrid.h"
#include "../../../Header Files/Core/EngineManager.h"

InfiniteGrid::InfiniteGrid() : quadVAO(0), quadVBO(0), gridShader(nullptr) {}

InfiniteGrid::~InfiniteGrid() {
    if (quadVAO != 0) {
        glDeleteVertexArrays(1, &quadVAO);
    }
    if (quadVBO != 0) {
        glDeleteBuffers(1, &quadVBO);
    }
}

void InfiniteGrid::Setup() {
    gridShader = std::make_unique<Shader>("Resources/Shaders/grid_vertex.txt", "Resources/Shaders/grid_fragment.txt");

    float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,

        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void InfiniteGrid::Draw(glm::vec2 cameraPos, glm::vec2 screenSize, float zoom) {
    if (!gridShader) return;

    gridShader->use();

    gridShader->setVec4D("bgColor", EngineManager::getInstance().EngineSettings.backgroundColor);
    gridShader->setVec2D("u_cameraPos", cameraPos);
    gridShader->setVec2D("u_screenSize", screenSize);
    gridShader->setFloat("u_zoom", zoom);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}