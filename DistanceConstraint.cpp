#include "DistanceConstraint.h"

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
	ImGui::InputFloat("##Distance", &distance, 0.0f, 0.0f, "%.3f m");

	ImGui::Text("Retractable ");
	ImGui::SameLine();
	ImGui::Checkbox("##Retractable ", &retractable);

	ImGui::Text("Extendable ");
	ImGui::SameLine();
	ImGui::Checkbox("##Extendable ", &extendable);
}

void DistanceConstraint::ProcessConstraintDisplay() {
	if (!constraintDisplay) return;
	RenderComponent* rc = constraintDisplay->GetComponent<RenderComponent>();
	TransformComponent* tc = constraintDisplay->GetComponent<TransformComponent>();
	if (objectA.obj == nullptr || objectB.obj == nullptr || !canDrawConstraint) {
		rc->SetEnabled(false);
		return;
	}

	glm::vec2 topVert = glm::vec2(0);
	glm::vec2 botVert = glm::vec2(0);

	glm::vec3 top = objectA.obj->GetComponent<TransformComponent>()->GetTransformedPoint(attachPointA);
	topVert = tc->GetTransformedPoint(top, true);
	//Project world -> screen
	glm::vec3 bot = objectB.obj->GetComponent<TransformComponent>()->GetTransformedPoint(attachPointB);
	botVert = tc->GetTransformedPoint(bot, true);
	std::vector<float> vertices = {};

	std::vector<unsigned int> indices = {};

	float thickness = 0.01f;

	glm::vec2 dir = botVert - topVert;

	float length = glm::length(dir);
	if (length < 0.0001f) return;

	float nx = -dir.y / length;
	float ny = dir.x / length;

	float halfThickness = thickness * 0.5f;
	float offsetX = nx * halfThickness;
	float offsetY = ny * halfThickness;

	unsigned int vertexOffset = vertices.size() / 5;

	vertices.insert(vertices.end(), { topVert.x + offsetX, topVert.y + offsetY, 0.0f, 0.0f, 0.0f });
	vertices.insert(vertices.end(), { botVert.x + offsetX, botVert.y + offsetY,   0.0f, 1.0f, 0.0f });
	vertices.insert(vertices.end(), { topVert.x - offsetX, topVert.y - offsetY, 0.0f, 0.0f, 1.0f });
	vertices.insert(vertices.end(), { botVert.x - offsetX, botVert.y - offsetY,   0.0f, 1.0f, 1.0f });

	indices.push_back(vertexOffset + 0);
	indices.push_back(vertexOffset + 2);
	indices.push_back(vertexOffset + 1);

	indices.push_back(vertexOffset + 1);
	indices.push_back(vertexOffset + 2);
	indices.push_back(vertexOffset + 3);

	rc->UpdateShape(vertices, indices);

	if (objectA.obj != nullptr && objectB.obj != nullptr) {
		rc->SetEnabled(true);
	}
	else {
		rc->SetEnabled(false);
	}
}

std::shared_ptr<Constraint> DistanceConstraint::Clone() {
	std::shared_ptr<DistanceConstraint> constraint = std::make_shared<DistanceConstraint>(PhysicsBody(), PhysicsBody(), attachPointA, attachPointB, distance, retractable, extendable);
	constraint->CopyBaseFieldsFrom(this);
	return constraint;
}