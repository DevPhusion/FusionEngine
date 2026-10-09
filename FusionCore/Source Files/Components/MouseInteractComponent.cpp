#include "../../Header Files/Components/MouseInteractComponent.h"

bool MouseInteractComponent::ObjectSelected = false;
bool MouseInteractComponent::PressArbitrated = false;
std::vector<MouseInteractComponent*> MouseInteractComponent::Instances;

MouseInteractComponent::MouseInteractComponent(Object* parent) : ComponentBase<MouseInteractComponent>(parent) {
	Name = "Mouse Interact Component";
	Hidden = true;
}

void MouseInteractComponent::CopyTo(Object* other) {
	MouseInteractComponent* target = other->GetComponent<MouseInteractComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<MouseInteractComponent>(other));
		target = other->GetComponent<MouseInteractComponent>();
	}

	target->SetEnabled(Enabled);
}

void MouseInteractComponent::RegisterCallbacks() {
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
}

void MouseInteractComponent::UnregisterCallbacks() {
	InputManager::getInstance().RemoveMouseButtonCallback(mouseButtonCallbackID);
	InputManager::getInstance().RemoveCursorPositionCallback(cursorPosCallbackID);
	EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsModeChangedCallbackID);
	mouseButtonCallbackID = {};
	cursorPosCallbackID = {};
	physicsModeChangedCallbackID = -1;
}

void MouseInteractComponent::Activate() {
	Component::Activate();
	RegisterCallbacks();
	if (std::find(Instances.begin(), Instances.end(), this) == Instances.end())
		Instances.push_back(this);
}

void MouseInteractComponent::Deactivate() {
	Component::Deactivate();
	UnregisterCallbacks();
	Instances.erase(std::remove(Instances.begin(), Instances.end(), this), Instances.end());

	if (Selected) {
		Selected = false;
		ObjectSelected = false;
	}
	isEditingViaMouse = false;

	if (parent && parent->HasComponent<RigidBodyComponent>()) {
		parent->GetComponent<RigidBodyComponent>()->isDragging = false;
	}
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
	Deactivate();
}

void MouseInteractComponent::SetSelectedPolygon(Object* obj, bool enable) {
	if (EngineManager::getInstance().isPlayer) return;
	if (Renderer::getInstance().gizmos->isDragging) return;

	if (!enable) {
		if (EditorManager::getInstance().selectedObject == obj) {
			EditorManager::getInstance().SetSelectedObject(nullptr);
		}
	}
	else {
		if (this != nullptr && Inspectable) {
			EditorManager::getInstance().SetSelectedObject(obj);
		}
		else if (this == nullptr) {
			EditorManager::getInstance().SetSelectedObject(obj);
		}
	}

}

bool MouseInteractComponent::IsCursorInside() {
	if (!parent || !Enabled) return false;
	TransformComponent* t = parent->GetComponent<TransformComponent>();
	if (!t) return false;

	glm::vec3 mousePos = t->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);
	if (parent->HasComponent<RenderComponent>())
		return parent->GetComponent<RenderComponent>()->IsInsideShape(mousePos);
	if (parent->HasComponent<EditorRenderComponent>())
		return parent->GetComponent<EditorRenderComponent>()->IsInsideShape(mousePos);
	return false;
}

float MouseInteractComponent::CursorDistance() {
	TransformComponent* t = parent->GetComponent<TransformComponent>();
	glm::vec3 origin = t->ProjectToWorld(glm::vec3(0.0f));
	return glm::length(glm::vec2(origin) - glm::vec2(InputManager::glX, InputManager::glY));
}

int MouseInteractComponent::HierarchyDepth() {
	int depth = 0;
	for (Object* p = parent->parent; p; p = p->parent) depth++;
	return depth;
}

std::vector<Object*> MouseInteractComponent::GetObjectsUnderCursor() {
	std::vector<MouseInteractComponent*> hits;
	for (MouseInteractComponent* c : Instances) {
		if (!c->parent || c->parent->hidden) continue;
		if (c->IsCursorInside()) hits.push_back(c);
	}

	std::sort(hits.begin(), hits.end(), [](MouseInteractComponent* a, MouseInteractComponent* b) {
		float da = a->CursorDistance(), db = b->CursorDistance();
		if (std::fabs(da - db) > 1e-3f) return da < db;
		return a->HierarchyDepth() < b->HierarchyDepth();
		});

	std::vector<Object*> result;
	result.reserve(hits.size());
	for (MouseInteractComponent* c : hits) result.push_back(c->parent);
	return result;
}

MouseInteractComponent* MouseInteractComponent::PickUnderCursor() {
	constexpr float eps = 1e-3f;
	Object* selected = EditorManager::getInstance().selectedObject;

	MouseInteractComponent* best = nullptr;
	float bestDist = 0.0f;
	int bestDepth = 0;

	for (MouseInteractComponent* c : Instances) {
		if (!c->IsCursorInside()) continue;

		if (selected && c->parent == selected) return c;

		float d = c->CursorDistance();
		int depth = c->HierarchyDepth();

		bool better = !best
			|| d < bestDist - eps
			|| (std::fabs(d - bestDist) <= eps && depth < bestDepth);

		if (better) {
			best = c;
			bestDist = d;
			bestDepth = depth;
		}
	}
	return best;
}

void MouseInteractComponent::ApplyMouseSelection() {
	Selected = true;
	ObjectSelected = true;

	if (EngineManager::getInstance().EngineSettings.physicsInteract
		&& EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
		if (parent->HasComponent<RigidBodyComponent>())
			parent->GetComponent<RigidBodyComponent>()->isDragging = true;
		if (parent->HasComponent<SoftBodyComponent>())
			parent->GetComponent<SoftBodyComponent>()->isDragging = true;
	}
	SetSelectedPolygon(parent, true);
	EditorManager::getInstance().BeginEdit({ parent });
	isEditingViaMouse = true;
}

void MouseInteractComponent::FindSelectedPolygon(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		PressArbitrated = false;
	}

	if (EngineManager::getInstance().EngineInteractMode != EngineManager::InteractMode::EditorSelect || !Enabled) {
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if (ObjectSelected || PressArbitrated) return;
		if (parent == nullptr) return;

		PressArbitrated = true;
		MouseInteractComponent* winner = PickUnderCursor();

		if (winner) {
			winner->ApplyMouseSelection();
		}
		else {
			for (MouseInteractComponent* c : Instances) {
				if (c->parent) c->SetSelectedPolygon(c->parent, false);
			}
		}
	}

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS
		&& EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate
		&& !Selected && EngineManager::getInstance().EngineSettings.physicsInteract) {
		if (parent->HasComponent<RigidBodyComponent>()) {
			parent->GetComponent<RigidBodyComponent>()->isDragging = true;
		}

		if (parent->HasComponent<SoftBodyComponent>()) {
			SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
			sb->isDragging = true;
		}

		Selected = true;
	}
}

void MouseInteractComponent::DragPolygon(double xpos, double ypos) {
	if (parent == nullptr) return;

	bool holding = InputManager::mouseLeftHold || InputManager::mouseRightHold;

	if (holding) {
		if (!Enabled) return;   

		if (EngineManager::getInstance().EngineInteractMode == EngineManager::InteractMode::AddVertex) {
			return;
		}

		if (Selected && isEditingViaMouse && EngineManager::getInstance().EnginePhysicsMode != EngineManager::PhysicsMode::Simulate) {
			GizmosMode gizmoMode = Renderer::getInstance().gizmos->currentGizmosMode;
			if (gizmoMode == GizmosMode::Rotate || gizmoMode == GizmosMode::Scale) {
				return;
			}

			TransformComponent* trans = parent->GetComponent<TransformComponent>();
			glm::vec3 modelPos = trans->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);
			glm::vec3 worldPos = trans->ProjectToWorld(modelPos);
			trans->UpdateWorldPosition(worldPos);
		}
	}
	else {
		if (isEditingViaMouse) {
			EditorManager::getInstance().EndEdit({ parent });
			isEditingViaMouse = false;
		}
		Selected = false;
		ObjectSelected = false;
		PressArbitrated = false;

		if (EngineManager::getInstance().EngineSettings.physicsInteract && parent->HasComponent<RigidBodyComponent>()) {
			parent->GetComponent<RigidBodyComponent>()->isDragging = false;
		}

		if (parent->HasComponent<SoftBodyComponent>()) {
			SoftBodyComponent* sb = parent->GetComponent<SoftBodyComponent>();
			sb->isDragging = false;
		}
	}
}

void MouseInteractComponent::OnPhysicsModeChanged() {
	if (parent->HasComponent<FluidComponent>()) {
		if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
			SetEnabled(false);
		}
		else if (parent->GetComponent<FluidComponent>()->Enabled) {
			SetEnabled(true);
		}
	}
}