#pragma once
#include "TransformComponent.h"

enum class GizmosMode {
	Move,
	Rotate,
	Scale
};

enum class GizmosAxis {
	None,
	X,
	Y,
	Rotate
};

class Gizmos
{
public:
	Gizmos();
	~Gizmos();

	GizmosMode currentGizmosMode = GizmosMode::Move;
	GizmosAxis selectedAxis = GizmosAxis::None;
	bool isDragging = false;

	void Initialize();
	void SwitchMode(GizmosMode mode) { currentGizmosMode = mode; }
	void DrawFilledQuad(glm::vec3 center, glm::vec3 size, glm::vec4 color, bool screenSpace = false);
	void UpdateGizmos();

	void OnMouseButton(int button, int action, int mods);
	void OnCursorPosition(double xpos, double ypos);

	glm::vec3 ScreenToWorld(double xpos, double ypos);
	GizmosAxis HitTestMoveScale(glm::vec3 origin);
	bool HitTestCircle(glm::vec3 origin);
	float DistancePointToSegment(glm::vec3 p, glm::vec3 a, glm::vec3 b);
	float GetAngle(glm::vec3 origin, glm::vec3 point);

	void DrawMoveGizmos();
	void DrawRotateGizmos();
	void DrawScaleGizmos();
	void DrawSquareArrow(glm::vec3 origin, glm::vec3 direction, float length, glm::vec4 color, float thickness = 1.0f, float squareSize = 0.05f, bool screenSpace = false);
private:
	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;

	glm::vec3 currentMouseWorld = glm::vec3(0.0f);
	glm::vec3 dragStartMouseWorld = glm::vec3(0.0f);
	glm::vec3 dragStartObjectPosition = glm::vec3(0.0f);
	glm::vec3 dragStartObjectScale = glm::vec3(1.0f);
	float dragStartAngleOffset = 0.0f;
	float dragStartObjectRotation = 0.0f;

	const float hitPadding = 0.05f;

	unsigned int gizmoVAO = 0;
	unsigned int gizmoVBO = 0;
	bool isInitialized = false;

	const float axisLength = 0.25f;
	const float circleRadius = 0.25f;
	const glm::vec4 xAxisColor = glm::vec4(0.85f, 0.15f, 0.15f, 1.0f); 
	const glm::vec4 yAxisColor = glm::vec4(0.15f, 0.85f, 0.15f, 1.0f); 
	const glm::vec4 rotateColor = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f);   

	TransformComponent* GetSelectedTransform();
};