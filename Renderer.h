#pragma once
#include "Shader.h"
#include "Object.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "SoftBodyComponent.h"
#include "FluidComponent.h"
#include "PhysicsEngine.h"
#include "InfiniteGrid.h"
#include "Gizmos.h"
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
	Gizmos* gizmos;
	void Setup(std::vector<std::unique_ptr<Object>>* objects);
	void SetupGrid() { backgroundGrid.Setup(); }
	void Draw();
	void DrawLine(glm::vec3 p1, glm::vec3 p2, glm::vec4 color, float thickness = 1.0f, bool screenSpace = false);
	void DrawArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color, float thickness = 1.0f,
		float headLength = 0.15f, float headAngleDeg = 30.0f, bool screenSpace = false);
	void DrawCircle(glm::vec3 center, float radius, glm::vec4 color, int segments = 32, float thickness = 1.0f, bool screenSpace = false);
private:
	Renderer() = default;
	std::vector<std::unique_ptr<Object>>* allObjects;
};

