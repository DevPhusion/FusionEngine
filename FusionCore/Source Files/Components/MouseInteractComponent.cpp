#include "../../Header Files/Components/MouseInteractComponent.h"

bool MouseInteractComponent::ObjectSelected = false;

MouseInteractComponent::MouseInteractComponent(Object* parent, bool physicsInteract) : ComponentBase<MouseInteractComponent>(parent) {
	Name = "Mouse Interact Component";
	this->physicsInteract = physicsInteract;

	int priority = 0;
	if (parent->HasComponent<RenderComponent>()) {
		priority = parent->GetComponent<RenderComponent>()->z_index;
	}
	else if (parent->HasComponent<EditorRenderComponent>()) {
		priority = parent->GetComponent<EditorRenderComponent>()->z_index;
	}

	mouseButtonCallbackID = InputManager::getInstance().SetMouseButtonCallback([this](int button, int action, int mods) {this->FindSelectedPolygon(button, action, mods);}, priority);
	cursorPosCallbackID = InputManager::getInstance().SetCursorPositionCallback([this](double xpos, double ypos) {this->DragPolygon(xpos, ypos);}, priority);
	physicsModeChangedCallbackID = EngineManager::getInstance().AddPhysicsModeChangedEvent([this]() {this->OnPhysicsModeChanged();});
	Hidden = true;
}

void MouseInteractComponent::CopyTo(Object* other) {
	MouseInteractComponent* target = other->GetComponent<MouseInteractComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<MouseInteractComponent>(other, physicsInteract));
		target = other->GetComponent<MouseInteractComponent>();
	}

	target->physicsInteract = physicsInteract;
}

std::unique_ptr<Component> MouseInteractComponent::Clone(Object* parent) {
	std::unique_ptr<MouseInteractComponent> comp = std::make_unique<MouseInteractComponent>(parent, physicsInteract);
	comp->SetEnabled(false);
	return comp;
}

void MouseInteractComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
}
void MouseInteractComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
}

void MouseInteractComponent::ProcessInspectorUI() {
	return;
}

void MouseInteractComponent::OnDelete() {
	InputManager::getInstance().RemoveMouseButtonCallback(mouseButtonCallbackID);
	InputManager::getInstance().RemoveCursorPositionCallback(cursorPosCallbackID);
	EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsModeChangedCallbackID);
}

void MouseInteractComponent::SetSelectedPolygon(Object* obj, bool enable) {
	if (Renderer::getInstance().gizmos->isDragging) return;

	if (!enable) {
		if (obj->HasComponent<VertexComponent>()) {
			if (obj->GetComponent<VertexComponent>()->GetSelectedVertex() != -1) {
				return;
			}
			obj->GetComponent<VertexComponent>()->SetEnabled(false);
		}
		if (EditorManager::getInstance().selectedObject == obj) {
			EditorManager::getInstance().SetSelectedObject(nullptr);
		}
	}
	else {
		if (obj->HasComponent<VertexComponent>() && EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
			obj->GetComponent<VertexComponent>()->SetEnabled(true);
		}
		if (this != nullptr && Inspectable) {
			EditorManager::getInstance().SetSelectedObject(obj);
		}
		else if (this == nullptr) {
			EditorManager::getInstance().SetSelectedObject(obj);
		}
	}

}

void MouseInteractComponent::FindSelectedPolygon(int button, int action, int mods) {
	if (EngineManager::getInstance().EngineInteractMode != EngineManager::InteractMode::EditorSelect || !Enabled) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {

		if (ObjectSelected) {
			if (parent->HasComponent<VertexComponent>()) {
				if (parent->GetComponent<VertexComponent>()->GetSelectedVertex() != -1) {
					return;
				}
				parent->GetComponent<VertexComponent>()->SetEnabled(false);
			}
			return;
		}

		if (parent == nullptr)
			return;

		glm::vec3 mousePos = parent->GetComponent<TransformComponent>()->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);

		bool inside = false;
		if (parent->HasComponent<RenderComponent>()) {
			inside = parent->GetComponent<RenderComponent>()->IsInsideShape(mousePos);
		}
		else if (parent->HasComponent<EditorRenderComponent>()) {
			inside = parent->GetComponent<EditorRenderComponent>()->IsInsideShape(mousePos);
		}

		if (inside) {
			Selected = true;
			ObjectSelected = true;

			if (physicsInteract && EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
				if (parent->HasComponent<RigidBodyComponent>()) {
					if (mouseDragForce != nullptr) {
						PhysicsEngine::getInstance().UnRegisterForce(parent, mouseDragForce);
					}
					mouseDragForce = new MouseDrag(150.0f, 24.5f);
					PhysicsEngine::getInstance().RegisterForce(parent, mouseDragForce);
				}
				if (parent->HasComponent<SoftBodyComponent>()) {
					parent->GetComponent<SoftBodyComponent>()->isDragging = true;
				}
			}
			SetSelectedPolygon(parent, true);
		}
		else {
			SetSelectedPolygon(parent, false);
		}
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		if (!Selected && physicsInteract && EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
			if (parent->HasComponent<RigidBodyComponent>()) {
				if (mouseDragForce != nullptr) {
					PhysicsEngine::getInstance().UnRegisterForce(parent, mouseDragForce);
				}

				mouseDragForce = new MouseDrag(150.0f, 24.5f);
				PhysicsEngine::getInstance().RegisterForce(parent, mouseDragForce);
			}

			if (parent->HasComponent<SoftBodyComponent>()) {
				SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
				sb->isDragging = true;
			}
		}
		Selected = true;
	}
}

void MouseInteractComponent::DragPolygon(double xpos, double ypos) {
	if (parent == nullptr || !Enabled)
		return;

	if (InputManager::mouseLeftHold || InputManager::mouseRightHold) 
	{
		if (parent->HasComponent<VertexComponent>()) {
			if (parent->GetComponent<VertexComponent>()->selectedIndex != -1) {
				return;
			}
		}

		if (Selected && EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
			GizmosMode gizmoMode = Renderer::getInstance().gizmos->currentGizmosMode;
			if (gizmoMode == GizmosMode::Rotate || gizmoMode == GizmosMode::Scale) {
				return;
			}
			
			TransformComponent* trans = parent->GetComponent<TransformComponent>();
			// project to model space
			glm::vec3 modelPos = trans->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);

			//project to world space
			glm::vec3 worldPos = trans->ProjectToWorld(modelPos);
			trans->UpdateWorldPosition(worldPos);
		}
	}
	else {
		Selected = false;
		ObjectSelected = false;
		if (mouseDragForce != nullptr && physicsInteract) {
			PhysicsEngine::getInstance().UnRegisterForce(parent, mouseDragForce);
			mouseDragForce = nullptr;
		}
		
		if (parent->HasComponent<SoftBodyComponent>()) {
			SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
			sb->isDragging = false;
		}
	}
}

void MouseInteractComponent::OnPhysicsModeChanged() {
	if (parent->HasComponent<VertexComponent>() && EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
		parent->GetComponent<VertexComponent>()->SetEnabled(false);
	}
	if (parent->HasComponent<FluidComponent>()) {
		if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
			SetEnabled(false);
		}
		else if (parent->GetComponent<FluidComponent>()->Enabled) {
			SetEnabled(true);
		}
	}
}