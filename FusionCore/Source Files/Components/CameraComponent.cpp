#include "../../Header Files/Components/CameraComponent.h"
#include "../../Header Files/Core/Rendering/Renderer.h"

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

void CameraComponent::DrawDebug() {
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;

	glm::vec3 worldPos = tc->GetWorldPosition();
	float rot = tc->rotation;
	float aspect = EngineManager::getInstance().gameAspectRatio;
	float zoom = range;

	// Unit corners before scale/rotate - matches the ortho frustum
	// (-aspect..aspect, -1..1) used by Renderer's line-drawing projection.
	glm::vec2 localCorners[4] = {
		glm::vec2(-aspect, -1.0f), // bottom-left
		glm::vec2(aspect, -1.0f), // bottom-right
		glm::vec2(aspect,  1.0f), // top-right
		glm::vec2(-aspect,  1.0f)  // top-left
	};

	glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), rot, glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 worldCorners[4];
	for (int i = 0; i < 4; i++) {
		glm::vec4 scaled(localCorners[i] * zoom, 0.0f, 1.0f);
		worldCorners[i] = worldPos + glm::vec3(rotMat * scaled);
	}

	glm::vec4 color = isMain ? glm::vec4(1.0f, 0.8f, 0.0f, 1.0f)   
		: glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);  

	for (int i = 0; i < 4; i++) {
		Renderer::getInstance().DrawLine(worldCorners[i], worldCorners[(i + 1) % 4], color, 2.0f, false);
	}
}