#pragma once
#include "XPBDConstraint.h"
class XPBDTriAreaConstraint : public XPBDConstraint
{
public:
	XPBDTriAreaConstraint(PhysicsBody center, PhysicsBody a, PhysicsBody b, float compliance);
	XPBDTriAreaConstraint() = default;

	PhysicsBody center, a, b;
	float restArea;

	float GetArea();

	virtual void SolvePosition(float delta);
};

