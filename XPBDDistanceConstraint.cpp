#include "XPBDDistanceConstraint.h"

XPBDDistanceConstraint::XPBDDistanceConstraint(PhysicsBody objA, PhysicsBody objB, float restLength, float compliance, float damping) {
	this->objA = objA;
	this->objB = objB;
	this->restLength = restLength;
	this->compliance = compliance;
	this->damping = damping;
}

void XPBDDistanceConstraint::SolvePosition(float delta) {
    if (objA.invMass == nullptr || objB.invMass == nullptr) return;
    if (delta <= 0.0f) return;

    glm::vec3 relPos = *objB.position - *objA.position;
    float dist = glm::length(relPos);
    if (dist < 1e-6f) return;

    glm::vec3 n = relPos / dist;
    float C = dist - restLength;

    glm::vec3 gradA = -n;
    glm::vec3 gradB = n;

    float wSum = (*objA.invMass) * glm::length2(gradA) + (*objB.invMass) * glm::length2(gradB);
    if (wSum < 1e-9f) return;

    float alphaTilde = compliance / (delta * delta);
    float gamma = (compliance * damping) / delta;

    glm::vec3 vA = (*objA.position - *objA.prevPos) / delta;
    glm::vec3 vB = (*objB.position - *objB.prevPos) / delta;
    float vRel = glm::dot(gradA, vA) + glm::dot(gradB, vB);

    float numerator = -C - (alphaTilde * lambda) - (gamma * vRel * delta);
    float denom = (1.0f + gamma) * wSum + alphaTilde;

    float deltaLambda = numerator / denom;
    lambda += deltaLambda;

    *objA.position += (*objA.invMass) * gradA * deltaLambda;
    *objB.position += (*objB.invMass) * gradB * deltaLambda;
}