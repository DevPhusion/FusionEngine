#pragma once
#include "XPBDConstraint.h"
class XPBDAreaConstraint : public XPBDConstraint
{
public:
	XPBDAreaConstraint(std::vector<PhysicsBody> MassAggregate, float compliance);
	XPBDAreaConstraint() = default;

	std::vector<PhysicsBody> MassAggregate;
	float defaultArea = 0.0f;

	float GetArea(std::vector<PhysicsBody> MassAggregate);
	virtual void SolvePosition(float delta);
};

