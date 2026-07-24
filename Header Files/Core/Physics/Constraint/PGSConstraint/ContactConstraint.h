#pragma once
#include "Constraint.h"
#include "../../Collision/ContactID.h"

class ContactConstraint : public Constraint
{
public:
	ContactConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, ContactID id,
		glm::vec3 normal, float penetration, float restitution, float staticFriction, float dynamicFriction);
	ContactConstraint() = default;

	ContactID contactId;

	glm::vec3 normal;
	float penetration;
	float restitution;
	float staticFriction;
	float dynamicFriction;

	int normalRowOffsetIndex = 0;
	int frictionRowOffsetIndex = -1;
	float bounceThreshold = 1.0f;

	float cacheLambda = 0.0f;
	float cacheFrictionLambda = 0.0f;

	virtual void Prepare(std::vector<SolverRow>& rows, float delta);
	virtual void PostIterationClamp(std::vector<SolverRow>& allRows, int myRowIndex, int velocityIteration);
	virtual void PostSolve(std::vector<SolverRow>& allRows);
	virtual std::shared_ptr<Constraint> Clone();
};

