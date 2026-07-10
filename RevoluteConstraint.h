#pragma once
#include "Constraint.h"
class RevoluteConstraint : public Constraint
{
public:
	RevoluteConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB);
	RevoluteConstraint() = default;

	virtual void Prepare(std::vector<SolverRow>& rows, float delta);
};

