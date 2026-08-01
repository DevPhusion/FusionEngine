#pragma once
#include "../../Components/TransformComponent.h"
#include <vector>

class PolygonEditGizmos
{
public:
	PolygonEditGizmos();
	~PolygonEditGizmos() = default;

	bool IsEditing() const { return isEditing; }
	const std::vector<glm::vec3>& GetLocalVertices() const { return localVertices; }
	bool CanAddVertices() const { return allowAddVertices; }

	void BeginEdit(TransformComponent* transform, const std::vector<glm::vec3>& initialLocalVerts = {}, bool allowAdd = true);
	void EndEdit();

	void UpdateGizmos();

	void OnMouseButton(int button, int action, int mods);
	void OnCursorPosition(double xpos, double ypos);

private:
	bool isEditing = false;
	TransformComponent* targetTransform = nullptr;
	std::vector<glm::vec3> localVertices;

	int draggedIndex = -1;
	bool isDragging = false;
	bool allowAddVertices = true;

	glm::vec3 currentMouseWorld = glm::vec3(0.0f);

	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;

	const float handleScreenSize = 0.025f;
	const float hitPadding = 0.035f;
	const glm::vec4 handleColor = glm::vec4(0.9f, 0.85f, 0.2f, 1.0f);
	const glm::vec4 draggedHandleColor = glm::vec4(1.0f, 0.45f, 0.1f, 1.0f);
	const glm::vec4 outlineColor = glm::vec4(0.2f, 0.85f, 1.0f, 0.9f);

	glm::vec3 WorldToLocal(glm::vec3 worldPos);
	glm::vec3 LocalToWorld(glm::vec3 localPos);
	int HitTestHandle(glm::vec3 worldPos);
	void DrawHandles();
	void DrawOutline();
};