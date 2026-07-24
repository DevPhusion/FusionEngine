#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include "../../Core/Rendering/Shader.h"

class InfiniteGrid
{
private:
    unsigned int quadVAO;
    unsigned int quadVBO;
    std::unique_ptr<Shader> gridShader;
public:
    InfiniteGrid();
    ~InfiniteGrid();

    void Setup();

    void Draw(glm::vec2 cameraPos, glm::vec2 screenSize, float zoom);
};

