#include "XPBDProxyPointConstraint.h"

XPBDProxyPointConstraint::XPBDProxyPointConstraint(PhysicsBody point, PhysicsBody proxy, glm::vec3 restOffset, float compliance, float damping) {
	this->point = point;
	this->proxy = proxy;
	this->restOffset = restOffset;
	this->compliance = compliance;
	this->damping = damping;
}

void XPBDProxyPointConstraint::SolvePosition(float delta) {
	if (point.invMass == nullptr || proxy.position == nullptr || proxy.rotation == nullptr
		|| proxy.invMass == nullptr || proxy.invInertia == nullptr) return;
	if (delta <= 0.0f) return;

	float c = std::cos(*proxy.rotation);
	float s = std::sin(*proxy.rotation);
	glm::vec3 rotatedOffset(
		restOffset.x * c - restOffset.y * s,
		restOffset.x * s + restOffset.y * c,
		0.0f
	);
	glm::vec3 target = *proxy.position + rotatedOffset;

	glm::vec3 relPos = *point.position - target;
	float dist = glm::length(relPos);
	if (dist < 1e-6f) return;

	glm::vec3 n = relPos / dist;
	float C = dist;

	float torqueArm = rotatedOffset.x * n.y - rotatedOffset.y * n.x;

	float wPoint = (*point.invMass);
	float wProxyLinear = (*proxy.invMass);
	float wProxyAngular = (*proxy.invInertia) * torqueArm * torqueArm;

	float wSum = wPoint + wProxyLinear + wProxyAngular;
	if (wSum < 1e-9f) return;

	float alphaTilde = compliance / (delta * delta);
	float gamma = (compliance * damping) / delta;

	glm::vec3 vPoint = (*point.position - *point.prevPos) / delta;
	glm::vec3 vProxy = (proxy.prevPos != nullptr) ? (*proxy.position - *proxy.prevPos) / delta : glm::vec3(0.0f);
	float vRel = glm::dot(n, vPoint - vProxy);

	float numerator = -C - (alphaTilde * lambda) - (gamma * vRel * delta);
	float denom = (1.0f + gamma) * wSum + alphaTilde;

	float deltaLambda = numerator / denom;

	glm::vec3 fullCorrection = n * deltaLambda;

	const float maxStepAbs = 0.5f; 
	float corrLen = glm::length(fullCorrection) * std::max(wPoint, wProxyLinear);
	if (corrLen > maxStepAbs && corrLen > 1e-9f) {
		fullCorrection *= (maxStepAbs / corrLen);
		deltaLambda = glm::dot(fullCorrection, n);
	}

	lambda += deltaLambda;

	*point.position += wPoint * fullCorrection;

	*proxy.position -= wProxyLinear * fullCorrection;
	*proxy.rotation -= wProxyAngular * torqueArm * deltaLambda;
}