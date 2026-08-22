#include "../../Header Files/Components/CameraComponent.h"
#include "../../Header Files/Core/Rendering/Renderer.h"

CameraComponent::CameraComponent(Object* parent) : ComponentBase<CameraComponent>(parent) {
	Name = "Camera Component";
	EditorRenderComponent* erc = parent->GetComponent<EditorRenderComponent>();
	if (erc) {
		erc->SetTexture("Resources/Images/Camera.png");
	}
}

void CameraComponent::RegisterCallbacks() {
	physicsModeChangedID = EngineManager::getInstance().AddPhysicsModeChangedEvent(
		[this]() { OnPhysicsModeChanged(); });

	if (TransformComponent* tc = parent->GetComponent<TransformComponent>()) {
		transformCallbackID = tc->AddTransformCallback([this]() {
			if (isDrivingCamera) SyncCamera();
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

void CameraComponent::Activate() {
	if (isActive) return;   
	isActive = true;

	if (!Enabled) return;   

	RegisterCallbacks();

	if (isMain) {
		if (Camera::getInstance().mainCam && Camera::getInstance().mainCam != this) {
			Camera::getInstance().mainCam->isMain = false;
		}
		Camera::getInstance().mainCam = this;
	}

	OnPhysicsModeChanged();
}

void CameraComponent::Deactivate() {
	if (!isActive) return;
	isActive = false;

	if (Enabled) {
		UnregisterCallbacks();
	}

	if (Camera::getInstance().mainCam == this) {
		Camera::getInstance().mainCam = nullptr;
	}

	isDrivingCamera = false;  
}

void CameraComponent::OnPhysicsModeChanged() {
	Camera& cam = Camera::getInstance();

	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate && isMain) {
		isDrivingCamera = true;
		SyncCamera();
	}
	else {
		isDrivingCamera = false;
	}
}

void CameraComponent::SyncCamera() {
	if (!Enabled) return;

	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!isDrivingCamera) return;
	if (!tc) return;

	Camera& cam = Camera::getInstance();

	glm::vec3 camTranslation = cam.WorldPositionToCameraTranslation(tc->GetWorldPosition(), range, tc->rotation);

	cam.SetCameraPosition(camTranslation);
	cam.SetCameraRotation(tc->rotation);
	cam.SetCameraZoom(range);
}

void CameraComponent::SetRange(float range) {
	this->range = range;
	if (isDrivingCamera) Camera::getInstance().SetCameraZoom(range);
}

void CameraComponent::ProcessInspectorUI() {
	ImGui::Text("Is Main ");
	ImGui::SameLine();
	bool mainFlag = isMain;
	if (ImGui::Checkbox("##IsMain", &mainFlag)) {
		CameraComponent* previousMain = Camera::getInstance().mainCam;
		Object* previousMainOwner = (previousMain && previousMain != this) ? previousMain->parent : nullptr;

		std::vector<Object*> editRoots = { parent };
		if (previousMainOwner) editRoots.push_back(previousMainOwner);

		EditorManager::getInstance().BeginEdit(editRoots);

		isMain = mainFlag;
		EngineManager::getInstance().SceneChangeEvent();
		if (isMain) {
			if (previousMain && previousMain != this) {
				previousMain->isMain = false;
			}
			Camera::getInstance().mainCam = this;
		}
		else if (Camera::getInstance().mainCam == this) {
			Camera::getInstance().mainCam = nullptr;
		}

		EditorManager::getInstance().EndEdit(editRoots);
	}

	ImGui::Text("Range ");
	ImGui::SameLine();
	float r = range;
	if (ImGui::InputFloat("## Range", &r)) {
		SetRange(r < 0.0f ? 0.01f : r);
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent });
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}
}

void CameraComponent::OnDelete() {
	Deactivate();
}

void CameraComponent::SetEnabled(bool enabled) {
	Component::SetEnabled(enabled);

	if (!isActive) return;

	if (enabled) {
		RegisterCallbacks();
		if (isMain) {
			if (Camera::getInstance().mainCam && Camera::getInstance().mainCam != this) {
				Camera::getInstance().mainCam->isMain = false;
			}
			Camera::getInstance().mainCam = this;
		}
		OnPhysicsModeChanged();  
	}
	else {
		UnregisterCallbacks();
		if (Camera::getInstance().mainCam == this) {
			Camera::getInstance().mainCam = nullptr;
		}
	}
}

void CameraComponent::CopyTo(Object* other) {
	CameraComponent* target = other->GetComponent<CameraComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<CameraComponent>(other));
		target = other->GetComponent<CameraComponent>();
	}
	target->SetEnabled(Enabled);
	target->isMain = isMain;
	target->SetRange(range);
}

void CameraComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write(isMain);
	w.Write(range);
}

void CameraComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	isMain = r.Read<bool>();
	range = r.Read<float>();   
}

void CameraComponent::DrawDebug() {
	isMain = Camera::getInstance().mainCam == this;
	TransformComponent* tc = parent->GetComponent<TransformComponent>();
	if (!tc) return;

	glm::vec3 worldPos = tc->GetWorldPosition();
	float rot = tc->rotation;
	float aspect = EngineManager::getInstance().gameAspectRatio;
	float zoom = range;

	glm::vec2 localCorners[4] = {
		glm::vec2(-aspect, -1.0f),
		glm::vec2(aspect, -1.0f),
		glm::vec2(aspect,  1.0f),
		glm::vec2(-aspect,  1.0f)
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