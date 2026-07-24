#pragma once
#include "XPBDConstraint.h"
class XPBDContactConstraint : public XPBDConstraint
{
public:
	XPBDContactConstraint(PhysicsBody objA, PhysicsBody objB, glm::vec3 normal, float separation, float compliance, float staticFriction, float dynamicFriction);
	XPBDContactConstraint() = default;

	PhysicsBody objA;
	PhysicsBody objB;
	glm::vec3 normal;
	float separation;
	float compliance;
	float staticFriction;
	float dynamicFriction;
	float lambda[2] = { 0.0f, 0.0f };

	virtual void SolvePosition(float delta);
};

