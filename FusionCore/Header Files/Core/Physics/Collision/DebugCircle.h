#pragma once
#include  "../../EngineManager.h"
#include "../../Camera.h"

class DebugCircle
{
public:
	DebugCircle() = default;
    static DebugCircle& getInstance() {
        static DebugCircle instance;
        return instance;
    }

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    const int segments = 64; 

    void Initialize();
    void DrawCircle(glm::vec3 center, float radius, Shader shader);
};

