#include "../../Header Files/Components/CameraComponent.h"

CameraComponent::CameraComponent(Object* parent) : ComponentBase<CameraComponent>(parent) {
	Name = "Camera Component";
	RegisterCallbacks();
	Camera::getInstance().mainCam = this;
}

void CameraComponent::RegisterCallbacks() {
	physicsModeChangedID = EngineManager::getInstance().AddPhysicsModeChangedEvent(
		[this]() { OnPhysicsModeChanged(); });

	if (TransformComponent* tc = parent->GetComponent<TransformComponent>()) {
		transformCallbackID = tc->AddTransformCallback([this]() {
			if (isActive) SyncCamera();
			});
	}
}

void CameraComponent::UnregisterCallbacks() {
	if (physicsModeChangedID != -1) {
		EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsModeChangedID);
		physicsModeChangedID = -1;
	}
	if (TransformComponent* tc = parent->GetComponent<TransformComponent>()) {
		if (transformCallbackID != -1) tc->RemoveTransformCallback(transformCallbackID);
	}
	transformCallbackID = -1;
}

void CameraComponent::OnPhysicsModeChanged() {
	Camera& cam = Camera::getInstance();

	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
		isActive = true;
		SyncCamera();
	}
	else {
		isActive = false;
	}
}

void CameraComponent::SyncCamera() {
	if (!Enabled) return;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!isActive) return;
	if (!tc) return;

	Camera& cam = Camera::getInstance();

	glm::vec3 camTranslation = cam.WorldPositionToCameraTranslation(tc->GetWorldPosition(), range, tc->rotation);

	cam.SetCameraPosition(camTranslation);
	cam.SetCameraRotation(tc->rotation);
	cam.SetCameraZoom(range);
}

void CameraComponent::SetRange(float range) {
	this->range = range;
	if (isActive) Camera::getInstance().SetCameraZoom(range);
}

void CameraComponent::ProcessInspectorUI() {
	isMain = Camera::getInstance().mainCam == this;
	ImGui::Text("Is Main ");
	ImGui::SameLine();
	if (ImGui::Checkbox("##IsMain", &isMain)) {
		if (isMain)
			Camera::getInstance().mainCam = this;
		else
			Camera::getInstance().mainCam = nullptr;
	}
	ImGui::Text("Range ");
	ImGui::SameLine();
	float r = range;
	if (ImGui::InputFloat("## Range", &r)) {
		SetRange(r < 1.0f ? 1.0f : r);
	}
}

void CameraComponent::OnDelete() {
	UnregisterCallbacks();
	if (Camera::getInstance().mainCam == this)
		Camera::getInstance().mainCam = nullptr;
}

void CameraComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);
	if (enabled) {
		RegisterCallbacks();
		Camera::getInstance().mainCam = this;
		isMain = true;
	}
	else {
		UnregisterCallbacks();
	}
}

void CameraComponent::CopyTo(Object* other) {
	CameraComponent* target = other->GetComponent<CameraComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<CameraComponent>(other));
		target = other->GetComponent<CameraComponent>();
	}
	target->isMain = isMain;
	target->SetRange(range);
}

std::unique_ptr<Component> CameraComponent::Clone(Object* parent) {
	std::unique_ptr<CameraComponent> comp = std::make_unique<CameraComponent>(parent);
	comp->SetRange(range);
	comp->SetEnabled(false);
	comp->isMain = isMain;
	Camera::getInstance().mainCam = this;
	return comp;
}

void CameraComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(isMain);
	w.Write(range);
}

void CameraComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	isMain = r.Read<bool>();
	SetRange(r.Read<float>());
}