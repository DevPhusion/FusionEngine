#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/SpringConstraint.h"
#include "../../../../../Header Files/Core/Rendering/Renderer.h"

SpringConstraint::SpringConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB,
	float length, float stiffness, float damping) :
	Constraint(objectA, objectB, attachPointA, attachPointB) {
	this->length = length;
	this->stiffness = stiffness;
	this->damping = damping;
	this->Name = "Spring Constraint";
}

void SpringConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
	if (objectA.position == nullptr || objectB.position == nullptr) return;

	JacobianRow jacobian = JacobianRow();
	SolverRow row = SolverRow();

	glm::vec3 globalPointA = (objectA.pm != nullptr)
		? *objectA.position
		: glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

	glm::vec3 globalPointB = (objectB.pm != nullptr)
		? *objectB.position
		: glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

	glm::vec3 d = globalPointB - globalPointA;
	float currentDistance = glm::length(d);
	glm::vec3 d_hat = (currentDistance > 0.00001f) ? d / currentDistance : glm::vec3(0.0f);

	glm::vec3 rA = globalPointA - *objectA.position;
	glm::vec3 rB = globalPointB - *objectB.position;

	jacobian.linearA = d_hat;
	jacobian.linearB = -d_hat;
	jacobian.angularA = (rA.x * d_hat.y - rA.y * d_hat.x);
	jacobian.angularB = -(rB.x * d_hat.y - rB.y * d_hat.x);

	row.jacobian = jacobian;

	float k = 0.0f;

	if (objectA.invMass != nullptr && objectA.invInertia != nullptr) {
		k += *objectA.invMass * glm::length2(jacobian.linearA) + *objectA.invInertia * (jacobian.angularA * jacobian.angularA);
	}
	if (objectB.invMass != nullptr && objectB.invInertia != nullptr) {
		k += *objectB.invMass * glm::length2(jacobian.linearB) + *objectB.invInertia * (jacobian.angularB * jacobian.angularB);
	}

	float softnessCFM = 0.0f;
	float finalBeta = beta;

	if (stiffness > 0.0f && k > 0.0f) {
		softnessCFM = 1.0f / (delta * (stiffness + delta * damping));
		finalBeta = delta * stiffness * softnessCFM;

		k += softnessCFM;
	}

	row.effectiveMass = (k > 0.0f) ? 1.0f / k : 0.0f;
	row.softnessCFM = softnessCFM;

	float error = currentDistance - length;
	float rawBias = (finalBeta / delta) * error;

	float maxRecoveryVelocity = 5.0f;
	if (stiffness > 0.0f) {
		row.bias = rawBias;
	}
	else {
		row.bias = glm::clamp(rawBias, -maxRecoveryVelocity, maxRecoveryVelocity);
	}

	row.objectA = objectA;
	row.objectB = objectB;

	row.maxLambda = INFINITY;
	row.minLambda = -INFINITY;

	row.lambda = cacheLambda;
	row.parentConstraint = this;

	rows.push_back(row);
}

void SpringConstraint::ProcessInspectorUI(Object* parent) {
	Constraint::ProcessInspectorUI(parent);

	ImGui::Text("Rest length ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Distance", &length, 0.0f, 0.0f, "%.3f m")) {
		EngineManager::getInstance().EngineChangeEvent();
	}

	ImGui::Text("Stiffness ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Stiffness", &stiffness, 0.0f, 0.0f, "%.3f N/m")) {
		EngineManager::getInstance().EngineChangeEvent();
	}


	ImGui::Text("Damping ");
	ImGui::SameLine();
	if (ImGui::InputFloat("##Damping", &damping, 0.0f, 0.0f, "%.3f Ns/m")) {
		EngineManager::getInstance().EngineChangeEvent();
	}
}

void GenerateSegment(glm::vec2 start, glm::vec2 end, float thickness,
	std::vector<float>& vertices, std::vector<unsigned int>& indices) {

	glm::vec2 dir = end - start;

	float length = glm::length(dir);
	if (length < 0.0001f) return;

	float nx = -dir.y / length;
	float ny = dir.x / length;

	float halfThickness = thickness * 0.5f;
	float offsetX = nx * halfThickness;
	float offsetY = ny * halfThickness;

	unsigned int vertexOffset = vertices.size() / 5;

	vertices.insert(vertices.end(), { start.x + offsetX, start.y + offsetY, 0.0f, 0.0f, 0.0f });
	vertices.insert(vertices.end(), { end.x + offsetX, end.y + offsetY,   0.0f, 1.0f, 0.0f });
	vertices.insert(vertices.end(), { start.x - offsetX, start.y - offsetY, 0.0f, 0.0f, 1.0f });
	vertices.insert(vertices.end(), { end.x - offsetX, end.y - offsetY,   0.0f, 1.0f, 1.0f });

	indices.push_back(vertexOffset + 0);
	indices.push_back(vertexOffset + 2);
	indices.push_back(vertexOffset + 1);

	indices.push_back(vertexOffset + 1);
	indices.push_back(vertexOffset + 2);
	indices.push_back(vertexOffset + 3);
}

void SpringConstraint::DrawConstraintGizmo() {
	if (objectA.obj == nullptr || objectB.obj == nullptr) return;

	glm::vec3 top = GetAttachWorldA();
	glm::vec3 bot = GetAttachWorldB();

	glm::vec3 axis = bot - top;
	float totalLength = glm::length(axis);
	if (totalLength < 0.0001f) return;

	const int segmentsCount = 10;
	const float amplitude = 0.5f;
	const float thickness = 6.0f;
	const glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

	glm::vec3 dir = axis / totalLength;
	glm::vec3 perp = glm::vec3(-dir.y, dir.x, 0.0f);

	std::vector<glm::vec3> springPoints;
	springPoints.push_back(top);

	for (int i = 1; i < segmentsCount; i++) {
		float t = (float)i / (float)segmentsCount;
		glm::vec3 basePoint = top + dir * (totalLength * t);
		float sideSign = (i % 2 == 1) ? 1.0f : -1.0f;
		springPoints.push_back(basePoint + perp * (amplitude * sideSign));
	}

	springPoints.push_back(bot);

	for (size_t i = 0; i < springPoints.size() - 1; i++) {
		Renderer::getInstance().DrawLine(springPoints[i], springPoints[i + 1], color, thickness);
	}
}


std::shared_ptr<Constraint> SpringConstraint::Clone() {
	std::shared_ptr<SpringConstraint> constraint = std::make_shared<SpringConstraint>(PhysicsBody(), PhysicsBody(), attachPointA, attachPointB, length, stiffness, damping);
	constraint->CopyBaseFieldsFrom(this);
	return constraint;
}

void SpringConstraint::Serialize(BinaryWriter& w) {
	Constraint::Serialize(w);
	w.Write(length);
	w.Write(stiffness);
	w.Write(damping);
}

void SpringConstraint::Deserialize(BinaryReader& r) {
	Constraint::Deserialize(r);
	length = r.Read<float>();
	stiffness = r.Read<float>();
	damping = r.Read<float>();
}