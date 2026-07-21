#include "XPBDTriAreaConstraint.h"

XPBDTriAreaConstraint::XPBDTriAreaConstraint(PhysicsBody center, PhysicsBody a, PhysicsBody b, float compliance) {
	this->center = center;
	this->a = a;
	this->b = b;
	this->compliance = compliance;
	restArea = GetArea();
}

float XPBDTriAreaConstraint::GetArea() {
	glm::vec3 c = *center.position, p1 = *a.position, p2 = *b.position;
	return 0.5f * ((p1.x - c.x) * (p2.y - c.y) - (p2.x - c.x) * (p1.y - c.y));
}

void XPBDTriAreaConstraint::SolvePosition(float delta) {
    glm::vec3 c = *center.position, p1 = *a.position, p2 = *b.position;
    float C = GetArea() - restArea;

    glm::vec3 gradP1 = 0.5f * glm::vec3(p2.y - c.y, c.x - p2.x, 0.0f);
    glm::vec3 gradP2 = 0.5f * glm::vec3(c.y - p1.y, p1.x - c.x, 0.0f);
    glm::vec3 gradC = -(gradP1 + gradP2);

    float wSum = *a.invMass * glm::length2(gradP1)
        + *b.invMass * glm::length2(gradP2)
        + *center.invMass * glm::length2(gradC);
    if (wSum < 1e-9f) return;

    float alpha_tilde = compliance / (delta * delta);
    float dLambda = (-C - alpha_tilde * lambda) / (wSum + alpha_tilde);
    lambda += dLambda;

    *a.position += *a.invMass * gradP1 * dLambda;
    *b.position += *b.invMass * gradP2 * dLambda;
    *center.position += *center.invMass * gradC * dLambda;
}