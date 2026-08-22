#pragma once
#include "Shader.h"
#include "../../Objects/Object.h"
#include "../../Components/RenderComponent.h"
#include "../../Components/EditorRenderComponent.h"
#include "../../Components/TransformComponent.h"
#include "../../Components/SoftBodyComponent.h"
#include "../../Components/FluidComponent.h"
#include "../../Components/CameraComponent.h"
#include "../Physics/PhysicsEngine.h"
#include "../Editor/InfiniteGrid.h"
#include "../Editor/Gizmos.h"
#include "../Editor/PolygonEditGizmos.h"
#include "../Editor/ConstraintEditGizmos.h"
#include "../DebugTimer.h"
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
	PolygonEditGizmos* polygonEditGizmos;
	ConstraintEditGizmos* constraintEditGizmos;
	void Setup(std::vector<std::unique_ptr<Object>>* objects);
	void SetupGrid() { backgroundGrid.Setup(); }
	void Draw();
	void DrawLine(glm::vec3 p1, glm::vec3 p2, glm::vec4 color, float thickness = 1.0f, bool screenSpace = false);
	void DrawArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color, float thickness = 1.0f,
		float headLength = 0.15f, float headAngleDeg = 30.0f, bool screenSpace = false);
	void DrawCircle(glm::vec3 center, float radius, glm::vec4 color, int segments = 32, float thickness = 1.0f, bool screenSpace = false);
	void DrawFilledPolygon(const std::vector<glm::vec3>& worldPoints, glm::vec4 fillColor, glm::vec4 outlineColor, float outlineThickness = 1.5f);
	
	std::vector<unsigned char> CaptureSnapshot(int width, int height);
	GLuint RenderLiveViewFrame(int width, int height);

private:
	Renderer() = default;
	std::vector<std::unique_ptr<Object>>* allObjects;

	void EnsureLiveViewFramebuffer(int width, int height);
	GLuint liveViewFBO = 0, liveViewColorTex = 0, liveViewDepthRBO = 0;
	int liveViewFBOWidth = 0, liveViewFBOHeight = 0;

	void EnsureHeadlessFramebuffer(int width, int height);
	void EnsureAllRenderResourcesLoaded();
	GLuint headlessFBO = 0, headlessColorTex = 0, headlessDepthRBO = 0;
	int headlessFBOWidth = 0, headlessFBOHeight = 0;
};

