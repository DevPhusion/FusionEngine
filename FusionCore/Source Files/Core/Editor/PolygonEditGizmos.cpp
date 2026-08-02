#include "../../../Header Files/Core/Editor/PolygonEditGizmos.h"
#include "../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../Header Files/Core/Editor/Gizmos.h"
#include "../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../Header Files/Core/InputManager.h"
#include "../../../Header Files/Core/EngineManager.h"
#include <GLFW/glfw3.h>

PolygonEditGizmos::PolygonEditGizmos() {
	mouseButtonCallbackID = InputManager::getInstance().SetMouseButtonCallback(
		[this](int button, int action, int mods) { OnMouseButton(button, action, mods); }, 998);
	cursorPosCallbackID = InputManager::getInstance().SetCursorPositionCallback(
		[this](double xpos, double ypos) { OnCursorPosition(xpos, ypos); }, 998);

	EngineManager::getInstance().AddPhysicsModeChangedEvent([this]() {
		if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate && isEditing) {
			EndEdit();
		}
		});
}

void PolygonEditGizmos::BeginEdit(TransformComponent* transform, const std::vector<glm::vec3>& initialLocalVerts, VertexAddMode mode) {
	targetTransform = transform;
	localVertices = initialLocalVerts;
	addMode = mode;
	isEditing = true;
	isDragging = false;
	draggedIndex = -1;
	hasEdgePreview = false;
	previewEdgeIndex = -1;
}

void PolygonEditGizmos::EndEdit() {
	isEditing = false;
	targetTransform = nullptr;
	localVertices.clear();
	isDragging = false;
	draggedIndex = -1;
	addMode = VertexAddMode::None;
	hasEdgePreview = false;
	previewEdgeIndex = -1;
}


glm::vec3 PolygonEditGizmos::WorldToLocal(glm::vec3 worldPos) {
	if (!targetTransform) return worldPos;
	return targetTransform->ProjectToWorld(worldPos, true);
}

glm::vec3 PolygonEditGizmos::LocalToWorld(glm::vec3 localPos) {
	if (!targetTransform) return localPos;
	return targetTransform->ProjectToWorld(localPos, false);
}

int PolygonEditGizmos::HitTestHandle(glm::vec3 worldPos) {
	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;
	float threshold = hitPadding * zoom;

	for (int i = 0; i < (int)localVertices.size(); i++) {
		glm::vec3 handleWorld = LocalToWorld(localVertices[i]);
		float dist = glm::length(glm::vec2(worldPos.x, worldPos.y) - glm::vec2(handleWorld.x, handleWorld.y));
		if (dist <= threshold) return i;
	}
	return -1;
}

static glm::vec3 ClosestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
	glm::vec2 p2(p.x, p.y), a2(a.x, a.y), b2(b.x, b.y);
	glm::vec2 ab = b2 - a2;
	float lengthSq = glm::dot(ab, ab);
	float t = 0.0f;
	if (lengthSq > 1e-8f) {
		t = glm::dot(p2 - a2, ab) / lengthSq;
		t = glm::clamp(t, 0.0f, 1.0f);
	}
	glm::vec2 closest = a2 + ab * t;
	return glm::vec3(closest.x, closest.y, 0.0f);
}

int PolygonEditGizmos::FindClosestEdge(glm::vec3 worldPos, glm::vec3& outLocalPoint, float& outWorldDist) {
	int best = -1;
	float bestDistSq = INFINITY;
	glm::vec3 bestLocal(0.0f);

	int count = (int)localVertices.size();
	for (int i = 0; i < count; i++) {
		glm::vec3 aWorld = LocalToWorld(localVertices[i]);
		glm::vec3 bWorld = LocalToWorld(localVertices[(i + 1) % count]);

		glm::vec3 closestWorld = ClosestPointOnSegment(worldPos, aWorld, bWorld);
		glm::vec2 diff = glm::vec2(worldPos.x, worldPos.y) - glm::vec2(closestWorld.x, closestWorld.y);
		float distSq = glm::dot(diff, diff);

		if (distSq < bestDistSq) {
			bestDistSq = distSq;
			best = i;
			bestLocal = WorldToLocal(closestWorld);
		}
	}

	outLocalPoint = bestLocal;
	outWorldDist = std::sqrt(bestDistSq);
	return best;
}

void PolygonEditGizmos::UpdateEdgePreview() {
	hasEdgePreview = false;
	previewEdgeIndex = -1;

	if (addMode != VertexAddMode::InsertOnEdge) return;
	if (localVertices.size() < 2) return;
	if (HitTestHandle(currentMouseWorld) != -1) return; // hovering an existing vertex, don't preview

	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;
	float threshold = hitPadding * zoom;

	glm::vec3 closestLocal;
	float worldDist;
	int edge = FindClosestEdge(currentMouseWorld, closestLocal, worldDist);

	if (edge != -1 && worldDist <= threshold) {
		previewEdgeIndex = edge;
		previewLocalPos = closestLocal;
		hasEdgePreview = true;
	}
}

void PolygonEditGizmos::OnMouseButton(int button, int action, int mods) {
	if (!isEditing) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;
	if (EditorManager::getInstance().WindowHovered) return;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		int hit = HitTestHandle(currentMouseWorld);
		if (hit != -1) {
			isDragging = true;
			draggedIndex = hit;
		}
		else if (addMode == VertexAddMode::InsertOnEdge && hasEdgePreview) {
			localVertices.insert(localVertices.begin() + previewEdgeIndex + 1, previewLocalPos);
			hasEdgePreview = false;
			previewEdgeIndex = -1;
			NotifyChange();
		}
		else if (addMode == VertexAddMode::Append) {
			localVertices.push_back(WorldToLocal(currentMouseWorld));
			NotifyChange();
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		isDragging = false;
		draggedIndex = -1;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		int hit = HitTestHandle(currentMouseWorld);
		if (hit != -1 && localVertices.size() > 3) {
			localVertices.erase(localVertices.begin() + hit);
			NotifyChange();
		}
	}
}

void PolygonEditGizmos::OnCursorPosition(double xpos, double ypos) {
	if (!isEditing) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;

	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;
	currentMouseWorld = gizmos->ScreenToWorld(xpos, ypos);

	if (isDragging && draggedIndex != -1 && draggedIndex < (int)localVertices.size()) {
		localVertices[draggedIndex] = WorldToLocal(currentMouseWorld);
		NotifyChange();
		hasEdgePreview = false;
		return;
	}

	UpdateEdgePreview();
}

void PolygonEditGizmos::NotifyChange() {
	for (auto& [id, func] : changeCallbacks) {
		func(localVertices);
	}
}

int PolygonEditGizmos::AddChangeCallback(std::function<void(const std::vector<glm::vec3>&)> func) {
	currentChangeCallbackID += 1;
	changeCallbacks[currentChangeCallbackID] = func;
	return currentChangeCallbackID;
	
}

void PolygonEditGizmos::RemoveChangeCallback(int ID) {
	changeCallbacks.erase(ID);
}

void PolygonEditGizmos::DrawHandles() {
	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;

	for (int i = 0; i < (int)localVertices.size(); i++) {
		glm::vec3 worldPos = LocalToWorld(localVertices[i]);
		glm::vec4 color = (i == draggedIndex) ? draggedHandleColor : handleColor;
		gizmos->DrawFilledQuad(worldPos, glm::vec3(handleScreenSize, handleScreenSize, 1.0f), color, true);
	}

	if (hasEdgePreview) {
		glm::vec3 previewWorld = LocalToWorld(previewLocalPos);
		gizmos->DrawFilledQuad(previewWorld, glm::vec3(handleScreenSize, handleScreenSize, 1.0f), previewHandleColor, true);
	}
}

void PolygonEditGizmos::DrawOutline() {
	if (localVertices.size() < 2) return;

	for (int i = 0; i < (int)localVertices.size(); i++) {
		glm::vec3 a = LocalToWorld(localVertices[i]);
		glm::vec3 b = LocalToWorld(localVertices[(i + 1) % localVertices.size()]);
		Renderer::getInstance().DrawLine(a, b, outlineColor, 2.0f);
	}
}

void PolygonEditGizmos::UpdateGizmos() {
	if (!isEditing) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;

	DrawOutline();
	DrawHandles();
}