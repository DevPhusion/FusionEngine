#include "../../../../Header Files/Core/Physics/Forces/MouseDrag.h"

MouseDrag::MouseDrag(float k, float c) {
	this->k = k;
	this->c = c;
}

void MouseDrag::updateForce(Object* object, float delta) {
	TransformComponent* trans = object->GetComponent<TransformComponent>();
	RigidBodyComponent* phys = object->GetComponent<RigidBodyComponent>();
	
	float scaledK = k * (1 / phys->inverseMass);
	float scaledC = c * (1 / phys->inverseMass);
	
	// project to model space
	glm::vec3 modelPos = trans->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);

	//project to world space
	glm::vec3 MouseWorldPos = trans->ProjectToWorld(modelPos);
	glm::vec3 ObjWorldPos = trans->GetWorldPosition();

	glm::vec3 displacement = MouseWorldPos - ObjWorldPos;
	glm::vec3 objVel = phys->velocity;
	Force = (scaledK * displacement) - (scaledC * objVel);

	glm::vec3 interactCenter = object->GetComponent<RenderComponent>()->GetCenter();
	if (Force.x > 1) {
		interactCenter.x += 0.01f / Force.x;
	}
	if (Force.y > 1) {
		interactCenter.y += 0.01f / Force.y;
	}
	phys->AddForce(Force);
	//phys->AddForceAtBodyPoint(Force, interactCenter);
}

void MouseDrag::processDisplay(int index) {
	ImGuiTreeNodeFlags root_flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_DefaultOpen;

	if (ImGui::TreeNodeEx((void*)(intptr_t)index, root_flags, "Mouse Drag", index)) {
		ImGui::Text("k ");
		ImGui::SameLine();
		ImGui::InputFloat("## k", &k, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

		ImGui::Text("c ");
		ImGui::SameLine();
		ImGui::InputFloat("## c", &c, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

		ImGui::Text("Mouse Drag ");
		ImGui::SameLine();
		float force[] = { Force.x, Force.y };
		ImGui::InputFloat2("## Mouse Drag", force, "%.3f N", ImGuiInputTextFlags_ReadOnly);

		ImGui::TreePop();
	}
}