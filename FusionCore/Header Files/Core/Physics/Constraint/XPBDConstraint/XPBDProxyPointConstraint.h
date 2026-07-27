#pragma once
#include "XPBDConstraint.h"
class XPBDProxyPointConstraint : public XPBDConstraint
{
public:
	XPBDProxyPointConstraint(PhysicsBody point, PhysicsBody proxy, glm::vec3 restOffset, float compliance, float damping);

	PhysicsBody point;
	PhysicsBody proxy;
	glm::vec3 restOffset;

	virtual void SolvePosition(float delta);
};

