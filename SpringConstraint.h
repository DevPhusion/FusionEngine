#pragma once
#include "Constraint.h"
class SpringConstraint : public Constraint
{
public:
	SpringConstraint() = default;
	SpringConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB,
		float length, float stiffness = 0.0f, float damping = 0.0f);

	float length;
	float stiffness;
	float damping;

	virtual void ProcessConstraintDisplay();
	virtual void Prepare(std::vector<SolverRow>& rows, float delta);
	virtual void ProcessInspectorUI(Object* parent);
};



