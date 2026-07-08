#include "RevoluteConstraint.h"

RevoluteConstraint::RevoluteConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, float weightA, float weightB) :
	Constraint(objectA, objectB, attachPointA, attachPointB, weightA, weightB) {
	this->Name = "Revolute Constraint";
}

void RevoluteConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
	if (objectA.obj == nullptr || objectB.obj == nullptr) {
		return;
	}

	glm::vec3 globalPointA = (objectA.pm != nullptr)
		? *objectA.position
		: glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

	glm::vec3 globalPointB = (objectB.pm != nullptr)
		? *objectB.position
		: glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

	glm::vec3 rA = globalPointA - *objectA.position;
	glm::vec3 rB = globalPointB - *objectB.position;

	JacobianRow jacobianX = JacobianRow();
	SolverRow rowX = SolverRow();
	jacobianX.linearA = glm::vec3(weightA, 0.0f, 0.0f);
	jacobianX.linearB = glm::vec3(-weightB, 0.0f, 0.0f);
	jacobianX.angularA = -weightA * rA.y;
	jacobianX.angularB = weightB * rB.y;
	rowX.jacobian = jacobianX;

	JacobianRow jacobianY = JacobianRow();
	SolverRow rowY = SolverRow();
	jacobianY.linearA = glm::vec3(0.0f, weightA, 0.0f);
	jacobianY.linearB = glm::vec3(0.0f, -weightB, 0.0f);
	jacobianY.angularA = weightA * rA.x;
	jacobianY.angularB = -weightB * rB.x;
	rowY.jacobian = jacobianY;

	float invMassA = objectA.invMass ? *objectA.invMass : 0.0f;
	float invMassB = objectB.invMass ? *objectB.invMass : 0.0f;
	float invInertiaA = objectA.invInertia ? *objectA.invInertia : 0.0f;
	float invInertiaB = objectB.invInertia ? *objectB.invInertia : 0.0f;

	float kX = invMassA * glm::length2(jacobianX.linearA) + invInertiaA * jacobianX.angularA * jacobianX.angularA +
		invMassB * glm::length2(jacobianX.linearB) + invInertiaB * jacobianX.angularB * jacobianX.angularB;

	float kY = invMassA * glm::length2(jacobianY.linearA) + invInertiaA * jacobianY.angularA * jacobianY.angularA +
		invMassB * glm::length2(jacobianY.linearB) + invInertiaB * jacobianY.angularB * jacobianY.angularB;

	rowX.effectiveMass = (kX > 0.0f) ? 1.0f / kX : 0.0f;
	rowY.effectiveMass = (kY > 0.0f) ? 1.0f / kY : 0.0f;

	glm::vec3 positionError = globalPointB - globalPointA;

	float biasX = (beta / delta) * positionError.x;
	float biasY = (beta / delta) * positionError.y;

	rowX.bias = biasX;
	rowX.maxLambda = INFINITY;
	rowX.minLambda = -INFINITY;
	rowX.objectA = objectA;
	rowX.objectB = objectB;
	rowX.parentConstraint = this;

	rowY.bias = biasY;
	rowY.maxLambda = INFINITY;
	rowY.minLambda = -INFINITY;
	rowY.objectA = objectA;
	rowY.objectB = objectB;
	rowY.parentConstraint = this;

	rows.push_back(rowX);
	rows.push_back(rowY);
}

void RevoluteConstraint::WarmStartSoftBody() {
	if (objectA.obj == nullptr || objectB.obj == nullptr) return;
	if (objectA.pm == nullptr && objectB.pm == nullptr) return;

	glm::vec3 globalPointA = (objectA.pm != nullptr)
		? *objectA.position
		: glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

	glm::vec3 globalPointB = (objectB.pm != nullptr)
		? *objectB.position
		: glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

	glm::vec3 positionError = globalPointB - globalPointA;

	if (objectA.pm != nullptr) {
		*objectA.position += positionError * weightA;
	}
	if (objectB.pm != nullptr) {
		*objectB.position -= positionError * weightB;
	}
}