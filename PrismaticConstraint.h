#pragma once
#include "Constraint.h"
class PrismaticConstraint : public Constraint
{
public:
	PrismaticConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, glm::vec3 dir);
	PrismaticConstraint() = default;

	glm::vec3 dir;
	float cacheLambda[2] = { 0.0f, 0.0f };

	int linearRowOffset = 0;
	int thetaRowOffset = 0;

	virtual void SetObjectA(PhysicsBody obj);
	virtual void SetObjectB(PhysicsBody obj);

	virtual void Prepare(std::vector<SolverRow>& rows, float delta);
	virtual void PostSolve(std::vector<SolverRow>& allRows);
	virtual void ProcessInspectorUI(Object* parent);
	virtual void ProcessConstraintDisplay();
};

