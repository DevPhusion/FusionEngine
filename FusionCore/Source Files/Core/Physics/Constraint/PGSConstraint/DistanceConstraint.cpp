#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/DistanceConstraint.h"
#include "../../../../../Header Files/Core/Rendering/Renderer.h"

DistanceConstraint::DistanceConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, 
	float distance, bool extendable, bool retractable) :
	Constraint(objectA, objectB, attachPointA, attachPointB) {
	this->distance = distance;
	this->extendable = extendable;
	this->retractable = retractable;
	this->Name = "Distance Constraint";
}

void DistanceConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
	if (objectA.obj == nullptr || objectB.obj == nullptr) {
		return;
	}

	JacobianRow jacobian = JacobianRow();
	SolverRow row = SolverRow();

	glm::vec3 globalPointA = *objectA.transformMatrix * glm::vec4(attachPointA, 1);
	glm::vec3 globalPointB = *objectB.transformMatrix * glm::vec4(attachPointB, 1);

	glm::vec3 d = globalPointB - globalPointA;
	float currentDistance = glm::length(d);
	glm::vec3 d_hat = (currentDistance > 0.00001f) ? d / currentDistance : glm::vec3(0.0f);
	glm::vec3 rA = globalPointA - *objectA.position;
	glm::vec3 rB = globalPointB - *objectB.position;

	jacobian.linearA = d_hat;
	jacobian.linearB = -d_hat;
	jacobian.angularA = rA.x * d_hat.y - rA.y * d_hat.x;
	jacobian.angularB = -(rB.x * d_hat.y - rB.y * d_hat.x);

	row.jacobian = jacobian;

	float k = 0.0f;

	if (objectA.invMass != nullptr && objectA.invInertia != nullptr) {
		k += *objectA.invMass * glm::length2(jacobian.linearA) + *objectA.invInertia * (jacobian.angularA * jacobian.angularA);
	}
	if (objectB.invMass != nullptr && objectB.invInertia != nullptr) {
		k += *objectB.invMass * glm::length2(jacobian.linearB) + *objectB.invInertia * (jacobian.angularB * jacobian.angularB);
	}

	row.effectiveMass = (k > 0.0f) ? 1.0f / k : 0.0f;

	float error = currentDistance - distance;
	row.bias = (beta / delta) * error;

	row.objectA = objectA;
	row.objectB = objectB;

	row.maxLambda = INFINITY;
	row.minLambda = -INFINITY;
	if (retractable) row.minLambda = 0;
	if (extendable) row.maxLambda = 0;

	row.lambda = cacheLambda;
	row.parentConstraint = this;

	rows.push_back(row);
}

void DistanceConstraint::ProcessInspectorUI(Object* parent) {
	Constraint::ProcessInspectorUI(parent);

	ImGui::Text("Distance ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Distance", &distance, 0.0f, 0.0f, "%.3f m")) {
		EngineManager::getInstance().SceneChangeEvent();
	}
	if (ImGui::IsItemActivated()) {
		EditorManager::getInstance().BeginEdit({ parent }, true);
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) {
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Retractable ");
	ImGui::SameLine();
	bool retractFlag = retractable;
	if (ImGui::Checkbox("##Retractable ", &retractFlag)) {
		EditorManager::getInstance().BeginEdit({ parent }, true);
		retractable = retractFlag;
		EngineManager::getInstance().SceneChangeEvent();
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Text("Extendable ");
	ImGui::SameLine();
	bool extendFlag = extendable;
	if (ImGui::Checkbox("##Extendable ", &extendFlag)) {
		EditorManager::getInstance().BeginEdit({ parent }, true);
		extendable = extendFlag;
		EngineManager::getInstance().SceneChangeEvent();
		EditorManager::getInstance().EndEdit({ parent });
	}
}

void DistanceConstraint::DrawConstraintGizmo() {
	if (objectA.obj == nullptr || objectB.obj == nullptr) return;
	DrawConstraintLine(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 6.0f);
}

void DistanceConstraint::Serialize(BinaryWriter& w) {
	Constraint::Serialize(w);
	w.Write(distance);
	w.Write(extendable);
	w.Write(retractable);
}

void DistanceConstraint::Deserialize(BinaryReader& r) {
	Constraint::Deserialize(r);
	distance = r.Read<float>();
	extendable = r.Read<bool>();
	retractable = r.Read<bool>();
}