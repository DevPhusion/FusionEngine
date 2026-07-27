#pragma once
#include "XPBDConstraint.h"
class XPBDDistanceConstraint : public XPBDConstraint
{
public:
	XPBDDistanceConstraint(PhysicsBody objA, PhysicsBody objB, float restLength, float compliance, float damping = 0.0f);
	XPBDDistanceConstraint() = default;

	PhysicsBody objA;
	PhysicsBody objB;
	float restLength;

	virtual void SolvePosition(float delta);
};

