#pragma once
#include "Shader.h"
#include "Object.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "SoftBodyComponent.h"
#include "PhysicsEngine.h"
#include "InfiniteGrid.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include <vector>
#include<iostream>
class Renderer
{
public:
	Renderer(const Renderer&) = delete;
	void operator=(const Renderer&) = delete;

	static Renderer& getInstance() {
		static Renderer instance;
		return instance;
	}
	InfiniteGrid backgroundGrid = InfiniteGrid();
	void Setup(std::vector<std::unique_ptr<Object>>* objects);
	void SetupGrid() { backgroundGrid.Setup(); }
	void Draw();
	void DrawLine(glm::vec3 p1, glm::vec3 p2, glm::vec4 color);
	void DrawArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color,
		float headLength = 0.04f, float headAngleDeg = 25.0f);
private:
	Renderer() = default;
	std::vector<std::unique_ptr<Object>>* allObjects;
};

