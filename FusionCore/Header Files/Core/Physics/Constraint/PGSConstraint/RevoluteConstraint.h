#pragma once
#include "Constraint.h"
#include <numbers>

enum MotorMode {
	Off,
	Velocity,
	Position
};

class RevoluteConstraint : public Constraint
{
public:
	RevoluteConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB);
	RevoluteConstraint() = default;

	virtual void Prepare(std::vector<SolverRow>& rows, float delta);
	virtual void PostIterationClamp(std::vector<SolverRow>& allRows, int myRowIndex, int velocityIteration);
	virtual void ProcessInspectorUI(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	MotorMode motorMode = MotorMode::Off;
	float motorSpeed = 0.0f;
	float targetAngle = 0.0f;
	float servoGain = 10.0f;
	float maxMotorSpeed = 5.0f;
	float maxMotorTorque = 50.0f;

	bool limitsEnabled = false;
	float lowerLimit = -3.14159f;
	float upperLimit = 3.14159f;

	void SetVelocityMotor(float speed, float maxTorque);
	void SetPositionMotor(float targetAngle, float gain, float maxSpeed, float maxTorque);
	void DisableMotor() { motorMode = MotorMode::Off; }
	void SetLimits(float lower, float upper) { limitsEnabled = true; lowerLimit = lower; upperLimit = upper; }
	void DisableLimits() { limitsEnabled = false; }
	float GetAngle() const { return currentAngle; }

private:
	bool angleInitialised = false;
	float referenceAngle = 0.0f;
	float lastRawAngle = 0.0f;
	float currentAngle = 0.0f;
};

