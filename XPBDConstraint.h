#pragma once
#include "PhysicsBody.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
class XPBDConstraint
{
public:
	XPBDConstraint() = default;

	float lambda = 0.0f;
	float compliance = 0.0f;
	float damping = 0.0f;

	void ResetLambda() { lambda = 0; }
	virtual void SolvePosition(float delta) = 0;
};

