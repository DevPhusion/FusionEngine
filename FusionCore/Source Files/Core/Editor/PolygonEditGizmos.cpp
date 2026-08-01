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

void PolygonEditGizmos::BeginEdit(TransformComponent* transform, const std::vector<glm::vec3>& initialLocalVerts, bool allowAdd) {
	targetTransform = transform;
	localVertices = initialLocalVerts;
	allowAddVertices = allowAdd;
	isEditing = true;
	isDragging = false;
	draggedIndex = -1;
}

void PolygonEditGizmos::EndEdit() {
	isEditing = false;
	targetTransform = nullptr;
	localVertices.clear();
	isDragging = false;
	draggedIndex = -1;
	allowAddVertices = true;
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
		else if (allowAddVertices) {
			localVertices.push_back(WorldToLocal(currentMouseWorld));
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
	}
}

void PolygonEditGizmos::DrawHandles() {
	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;

	for (int i = 0; i < (int)localVertices.size(); i++) {
		glm::vec3 worldPos = LocalToWorld(localVertices[i]);
		glm::vec4 color = (i == draggedIndex) ? draggedHandleColor : handleColor;
		gizmos->DrawFilledQuad(worldPos, glm::vec3(handleScreenSize, handleScreenSize, 1.0f), color, true);
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