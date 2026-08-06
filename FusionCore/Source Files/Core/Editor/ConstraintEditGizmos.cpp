#include "../../../Header Files/Core/Editor/ConstraintEditGizmos.h"
#include "../../../Header Files/Core/Rendering/Renderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>

ConstraintEditGizmos::ConstraintEditGizmos() {
	mouseButtonCallbackID = InputManager::getInstance().SetMouseButtonCallback(
		[this](int button, int action, int mods) { OnMouseButton(button, action, mods); }, 997);
	cursorPosCallbackID = InputManager::getInstance().SetCursorPositionCallback(
		[this](double xpos, double ypos) { OnCursorPosition(xpos, ypos); }, 997);
}

void ConstraintEditGizmos::RegisterConstraint(Constraint* c) {
	if (std::find(registeredConstraints.begin(), registeredConstraints.end(), c) == registeredConstraints.end())
		registeredConstraints.push_back(c);
}

void ConstraintEditGizmos::UnregisterConstraint(Constraint* c) {
	registeredConstraints.erase(
		std::remove(registeredConstraints.begin(), registeredConstraints.end(), c),
		registeredConstraints.end());

	if (dragConstraint == c) {
		dragConstraint = nullptr;
		dragTarget = DragTarget::None;
	}
}

bool ConstraintEditGizmos::HitTestHandle(glm::vec3 worldPos, glm::vec3 handleWorldPos) {
	float zoom = Camera::getInstance().cameraZoom;
	if (zoom < 1e-6f) zoom = 1.0f;
	float threshold = hitPadding * zoom;
	float dist = glm::length(glm::vec2(worldPos.x, worldPos.y) - glm::vec2(handleWorldPos.x, handleWorldPos.y));
	return dist <= threshold;
}

void ConstraintEditGizmos::DrawHandle(glm::vec3 worldPos, const glm::vec4& color) {
	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;
	gizmos->DrawFilledQuad(worldPos, glm::vec3(handleScreenSize, handleScreenSize, 1.0f), color, true);
}

void ConstraintEditGizmos::OnMouseButton(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		dragConstraint = nullptr;
		dragTarget = DragTarget::None;
		return;
	}

	if (EngineManager::getInstance().EngineInteractMode != EngineManager::InteractMode::ConstraintEdit) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;
	if (EditorManager::getInstance().WindowHovered) return;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		for (Constraint* c : registeredConstraints) {
			if (c->IsAttachAEditing() && !c->UseCenterA() &&
				HitTestHandle(currentMouseWorld, c->GetAttachWorldA())) {
				dragConstraint = c;
				dragTarget = DragTarget::AttachA;
				return;
			}
			if (c->IsAttachBEditing() && !c->UseCenterB() &&
				HitTestHandle(currentMouseWorld, c->GetAttachWorldB())) {
				dragConstraint = c;
				dragTarget = DragTarget::AttachB;
				return;
			}
		}
	}
}

void ConstraintEditGizmos::OnCursorPosition(double xpos, double ypos) {
	Gizmos* gizmos = Renderer::getInstance().gizmos;
	if (!gizmos) return;
	currentMouseWorld = gizmos->ScreenToWorld(xpos, ypos);

	if (EngineManager::getInstance().EngineInteractMode != EngineManager::InteractMode::ConstraintEdit) return;
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;
	if (!dragConstraint) return;

	if (dragTarget == DragTarget::AttachA) dragConstraint->OnAttachAMoved(currentMouseWorld);
	else if (dragTarget == DragTarget::AttachB) dragConstraint->OnAttachBMoved(currentMouseWorld);
}

void ConstraintEditGizmos::DrawConstraintDisplays() {
	for (Constraint* c : registeredConstraints) {
		if (c->canDrawConstraint)
			c->DrawConstraintGizmo();
	}
}

void ConstraintEditGizmos::DrawPivotHandles() {
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) return;

	for (Constraint* c : registeredConstraints) {
		if (c->IsAttachAEditing() && !c->UseCenterA()) {
			bool dragging = (dragConstraint == c && dragTarget == DragTarget::AttachA);
			DrawHandle(c->GetAttachWorldA(), dragging ? draggedHandleColor : handleColorA);
		}
		if (c->IsAttachBEditing() && !c->UseCenterB()) {
			bool dragging = (dragConstraint == c && dragTarget == DragTarget::AttachB);
			DrawHandle(c->GetAttachWorldB(), dragging ? draggedHandleColor : handleColorB);
		}
	}
}