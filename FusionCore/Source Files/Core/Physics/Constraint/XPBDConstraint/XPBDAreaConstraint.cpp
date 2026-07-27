#include "../../../../../Header Files/Core/Physics/Constraint/XPBDConstraint/XPBDAreaConstraint.h"

XPBDAreaConstraint::XPBDAreaConstraint(std::vector<PhysicsBody> MassAggregate, float compliance) {
	this->MassAggregate = MassAggregate;
	this->compliance = compliance;
	this->defaultArea = GetArea(MassAggregate);
}

float XPBDAreaConstraint::GetArea(std::vector<PhysicsBody> MassAggregate) {
	float sum = 0.0f;
	for (int i = 0; i < MassAggregate.size(); i++)
	{
		glm::vec3 p1 = *MassAggregate[i].position;
		glm::vec3 p2 = *MassAggregate[(i + 1) % MassAggregate.size()].position;
		sum += p1.x * p2.y - p2.x * p1.y;
	}

	return 0.5 * sum;
}

void XPBDAreaConstraint::SolvePosition(float delta) {
	int n = MassAggregate.size();
	float C = GetArea(MassAggregate) - defaultArea;
	std::vector<glm::vec3> gradients;
	float wSum = 0.0f;
	for (int i = 0; i < n; i++)
	{
		glm::vec3 p1 = *MassAggregate[(i - 1 + n) % n].position;
		glm::vec3 p2 = *MassAggregate[(i + 1) % n].position;
		glm::vec3 grad = glm::vec3(
			0.5 * (p2.y - p1.y),
			0.5 * (p1.x - p2.x),
			0.0f
		);
		gradients.push_back(grad);

		wSum += *MassAggregate[i].invMass * glm::length2(grad);
	}

	if (wSum < 1e-9f) return;
	float alpha_tilde = compliance / (delta * delta);
	float delta_lambda = (-C - alpha_tilde * lambda) / (wSum + alpha_tilde);
	lambda += delta_lambda;

	for (int i = 0; i < n; i++)
	{
		*MassAggregate[i].position += *MassAggregate[i].invMass * gradients[i] * delta_lambda;
	}
}