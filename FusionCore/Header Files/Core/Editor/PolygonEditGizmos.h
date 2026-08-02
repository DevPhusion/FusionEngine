#pragma once
#include "../../Components/TransformComponent.h"
#include <vector>
#include <functional>

class PolygonEditGizmos
{
public:
	enum class VertexAddMode {
		None,
		Append,      
		InsertOnEdge  
	};

	PolygonEditGizmos();
	~PolygonEditGizmos() = default;

	bool IsEditing() const { return isEditing; }
	const std::vector<glm::vec3>& GetLocalVertices() const { return localVertices; }
	void BeginEdit(TransformComponent* transform,
		const std::vector<glm::vec3>& initialLocalVerts = {},
		VertexAddMode mode = VertexAddMode::Append);

	void EndEdit();

	void UpdateGizmos();

	void OnMouseButton(int button, int action, int mods);
	void OnCursorPosition(double xpos, double ypos);

	int AddChangeCallback(std::function<void(const std::vector<glm::vec3>&)> func);
	void RemoveChangeCallback(int ID);

	bool CanAddVertices() const { return addMode != VertexAddMode::None; }
	VertexAddMode GetAddMode() const { return addMode; }

private:
	VertexAddMode addMode = VertexAddMode::None;
	bool hasEdgePreview = false;
	int previewEdgeIndex = -1;
	glm::vec3 previewLocalPos{ 0.0f };
	glm::vec4 previewHandleColor = glm::vec4(1.0f, 0.95f, 0.3f, 0.5f);

	bool isEditing = false;
	TransformComponent* targetTransform = nullptr;
	std::vector<glm::vec3> localVertices;

	int draggedIndex = -1;
	bool isDragging = false;

	glm::vec3 currentMouseWorld = glm::vec3(0.0f);

	std::vector<int> mouseButtonCallbackID;
	std::vector<int> cursorPosCallbackID;

	std::unordered_map<int, std::function<void(const std::vector<glm::vec3>&)>> changeCallbacks;
	int currentChangeCallbackID = -1;

	const float handleScreenSize = 0.025f;
	const float hitPadding = 0.035f;
	const glm::vec4 handleColor = glm::vec4(0.9f, 0.85f, 0.2f, 1.0f);
	const glm::vec4 draggedHandleColor = glm::vec4(1.0f, 0.45f, 0.1f, 1.0f);
	const glm::vec4 outlineColor = glm::vec4(0.2f, 0.85f, 1.0f, 0.9f);

	void NotifyChange();

	void UpdateEdgePreview();
	int FindClosestEdge(glm::vec3 worldPos, glm::vec3& outLocalPoint, float& outWorldDist);
	glm::vec3 WorldToLocal(glm::vec3 worldPos);
	glm::vec3 LocalToWorld(glm::vec3 localPos);
	int HitTestHandle(glm::vec3 worldPos);
	void DrawHandles();
	void DrawOutline();
};