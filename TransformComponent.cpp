#include "TransformComponent.h"
#include "RigidBodyComponent.h"

TransformComponent::TransformComponent(Object* parent, Shader shader, glm::vec3 rotation_center) : ComponentBase<TransformComponent>(parent) {
	Name = "Transform Component";

	this->shader = shader;
	this->rotation_center = rotation_center;
	CanRemove = false;
	CanDisable = false;
	
	SetOriginTransform(Camera::getInstance().viewMatrixInverse);
	UpdateWorldPosition(GetWorldPosition());
}

void TransformComponent::CopyTo(Object* other) {
	TransformComponent* target = other->GetComponent<TransformComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<TransformComponent>(other, other->shader, rotation_center));
		target = other->GetComponent<TransformComponent>();
	}

	target->rotation = this->rotation;
	target->size = this->size;
	target->SetRotationCenter(rotation_center);
	target->SetOriginTransform(this->OriginTransform);
	target->UpdateWorldPosition(target->GetWorldPosition());

	Shape shapeCopy = parent->GetComponent<RenderComponent>()->currentShape;

	std::visit([&](auto&& s) {
		using T = std::decay_t<decltype(s)>;
		if constexpr (std::is_same_v<T, RectangleShape> || std::is_same_v<T, CircleShape>) {
			s.center = target->GetWorldPosition(); 
		}
		}, shapeCopy);

	other->GetComponent<RenderComponent>()->SetShape(shapeCopy);
	target->SetRotationCenter(other->GetComponent<RenderComponent>()->GetCenter());
}

void TransformComponent::ProcessInspectorUI() {
	ImGui::Text("Position ");
	ImGui::SameLine();
	float position[] = { worldPosition.x, worldPosition.y };
	if (ImGui::InputFloat2("## Position", position)) {
		UpdateWorldPosition(glm::vec3(position[0], position[1], 0));
	}
	ImGui::Text("Rotation ");
	ImGui::SameLine();
	float rotation = this->rotation;
	if (ImGui::SliderAngle("## Rotation", &rotation, -180.0f, 180.0f)) {
		Rotate(rotation);
	}

	ImGui::Text("Scale ");
	ImGui::SameLine();
	float size[] = { this->size.x, this->size.y };
	if (ImGui::InputFloat2("## Scale", size)) {
		Scale(glm::vec3(size[0], size[1], 1));
	}

}

void TransformComponent::OnDelete() {

}

glm::vec3 TransformComponent::GetWorldPosition() {
	return ProjectToWorld(rotation_center);
}

glm::mat4 TransformComponent::GetWorldMatrix() {
	if (worldMatrixDirty) {
		WorldMatrix = OriginTransform;
		WorldMatrix = glm::translate(WorldMatrix, rotation_center);
		WorldMatrix = glm::rotate(WorldMatrix, rotation, glm::vec3(0, 0, 1));
		WorldMatrix = glm::scale(WorldMatrix, size);
		WorldMatrix = glm::translate(WorldMatrix, -rotation_center);
		worldMatrixDirty = false;
	}
	return WorldMatrix;
}

glm::vec3 TransformComponent::ProjectToWorld(glm::vec3 point, bool inverseTransform) {
	glm::mat4 trans = GetWorldMatrix();
	glm::vec4 p = glm::vec4(point.x, point.y, point.z, 1.0f);
	if (inverseTransform) {
		return glm::vec3(glm::inverse(trans) * p);
	}
	return glm::vec3(trans * p);
}

void TransformComponent::UpdateWorldPosition(glm::vec3 targetWorldPos) {
	glm::vec4 center = glm::vec4(rotation_center.x, rotation_center.y, rotation_center.z, 1.0f);
	glm::mat4 trans = OriginTransform;
	trans = glm::translate(trans, rotation_center); // Translate to
	trans = glm::rotate(trans, rotation, glm::vec3(0, 0, 1));
	trans = glm::scale(trans, size);
	trans = glm::translate(trans, -rotation_center); // Translate back
	glm::vec4 currentWorldPos = trans * center;

	glm::vec3 currentPosVec3 = glm::vec3(currentWorldPos.x, currentWorldPos.y, currentWorldPos.z);
	glm::vec3 delta = targetWorldPos - currentPosVec3;

	glm::mat4 newOriginTransform = glm::translate(glm::mat4(1.0f), delta) * OriginTransform;
	
	SetOriginTransform(newOriginTransform);
	prevPos = worldPosition;
	worldPosition = GetWorldPosition();
}

glm::vec3 TransformComponent::GetTransformedPoint(glm::vec3 point, bool inverseTransform) {
	glm::vec4 originalPoint = glm::vec4(point.x, point.y, 1.0f, 1.0f);

	glm::mat4 trans = OriginTransform;
	trans = glm::translate(trans, rotation_center); // Translate to 0,0
	trans = glm::translate(trans, position);
	trans = glm::rotate(trans, rotation, glm::vec3(0, 0, 1));
	trans = glm::scale(trans, size);
	trans = glm::translate(trans, -rotation_center); // Translate back to original pos

	glm::vec4 transformedPoint = glm::vec4(1);

	if (inverseTransform) {
		transformedPoint =  glm::inverse(trans) * Camera::getInstance().viewMatrixInverse * originalPoint;
	}
	else {
		transformedPoint = Camera::getInstance().viewMatrix * trans * originalPoint;
	}
	
	return glm::vec3(transformedPoint.x, transformedPoint.y, 0.0f);
}

void TransformComponent::SetRotationCenter(glm::vec3 rotation_center) {
	this->rotation_center = rotation_center;
}

void TransformComponent::SetOriginTransform(glm::mat4 transform) {
	this->OriginTransform = transform;
	worldMatrixDirty = true;
	for (const auto& [id, func] : transformCallback) {
		func();
	}
}

void TransformComponent::Translate(glm::vec3 translation) {
	position = translation;
	worldMatrixDirty = true;
	for (const auto& [id, func] : transformCallback) {
		func();
	}
}

void TransformComponent::Rotate(float angle)
{
	rotation = angle;
	worldMatrixDirty = true;
	for (const auto& [id, func] : transformCallback) {
		func();
	}
}

void TransformComponent::Scale(glm::vec3 scale) {
	size = scale;
	worldMatrixDirty = true;
	for (const auto& [id, func] : transformCallback) {
		func();
	}
	RigidBodyComponent* pc = parent->GetComponent<RigidBodyComponent>();
	if (pc) {
		pc->CalculateInertia();
	}
}

int TransformComponent::AddTransformCallback(std::function<void()> func) {
	CurrentTransformCallbackID += 1;
	this->transformCallback[CurrentTransformCallbackID] = func;
	return CurrentTransformCallbackID;
}

void TransformComponent::RemoveTransformCallback(int ID) {
	this->transformCallback.erase(ID);
}

void TransformComponent::ProcessTransform() {
	this->transform = OriginTransform;
	this->transform = glm::translate(this->transform, rotation_center); // Translate to 0,0

	this->transform = glm::translate(this->transform, position);
	this->transform = glm::rotate(this->transform, rotation, glm::vec3(0, 0, 1));
	this->transform = glm::scale(this->transform, size);

	this->transform = glm::translate(this->transform, -rotation_center); // Translate back to original pos
	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
	this->shader.setMat4D("projection", projection);
	this->shader.setMat4D("transform", this->transform);
	this->shader.setMat4D("view", Camera::getInstance().viewMatrix);

	if (worldPosition != GetWorldPosition()) {
		prevPos = GetWorldPosition();
		UpdateWorldPosition(worldPosition);
	}
}
