#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/RevoluteConstraint.h"
#include "../../../../../Header Files/Core/Editor/EditorField.h"

RevoluteConstraint::RevoluteConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB) :
	Constraint(objectA, objectB, attachPointA, attachPointB) {
	this->Name = "Revolute Constraint";
}

void RevoluteConstraint::SetVelocityMotor(float speed, float maxTorque) {
	motorMode = MotorMode::Velocity;
	motorSpeed = speed;
	maxMotorTorque = maxTorque;
}

void RevoluteConstraint::SetPositionMotor(float target, float gain, float maxSpeed, float maxTorque) {
	motorMode = MotorMode::Position;
	targetAngle = target;
	servoGain = gain;
	maxMotorSpeed = maxSpeed;
	maxMotorTorque = maxTorque;
}

void RevoluteConstraint::ProcessInspectorUI(Object* parent) {
	Constraint::ProcessInspectorUI(parent);

	auto inputDegrees = [&](const char* label, const char* id, float& radians, const char* fmt, auto&& afterChange) {
		float deg = glm::degrees(radians);
		EditorField::InputFloatScene(parent, label, id, &deg, [&] {
			radians = glm::radians(deg);
			afterChange();
			}, fmt);
		};
	auto noop = [] {};

	ImGui::Separator();
	ImGui::Text("Motor ");
	ImGui::SameLine();
	const char* modeNames[] = { "Off", "Velocity", "Position" };
	int currentMode = (int)motorMode;
	EditorField::ActionScene(parent,
		ImGui::Combo("##MotorMode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames)),
		[&] { motorMode = (MotorMode)currentMode; });

	if (motorMode == MotorMode::Velocity) {
		inputDegrees("Speed ", "##MotorSpeed", motorSpeed, "%.2f deg/s", noop);
	}
	else if (motorMode == MotorMode::Position) {
		inputDegrees("Target Angle ", "##TargetAngle", targetAngle, "%.1f deg", noop);

		EditorField::InputFloatScene(parent, "Gain ", "##ServoGain", &servoGain,
			[&] { servoGain = std::max(servoGain, 0.0f); }, "%.2f 1/s");

		inputDegrees("Max Speed ", "##MaxMotorSpeed", maxMotorSpeed, "%.2f deg/s",
			[&] { maxMotorSpeed = std::max(maxMotorSpeed, 0.0f); });
	}

	if (motorMode != MotorMode::Off) {
		EditorField::InputFloatScene(parent, "Max Torque ", "##MaxMotorTorque", &maxMotorTorque,
			[&] { maxMotorTorque = std::max(maxMotorTorque, 0.0f); }, "%.2f N*m");
	}

	ImGui::Separator();
	bool limitsFlag = limitsEnabled;
	EditorField::CheckboxScene(parent, "Limits ", "##LimitsEnabled", &limitsFlag,
		[&] { limitsEnabled = limitsFlag; });

	if (limitsEnabled) {
		inputDegrees("Lower ", "##LowerLimit", lowerLimit, "%.1f deg",
			[&] { upperLimit = std::max(upperLimit, lowerLimit); });
		inputDegrees("Upper ", "##UpperLimit", upperLimit, "%.1f deg",
			[&] { lowerLimit = std::min(lowerLimit, upperLimit); });
	}

	if (angleInitialised && (motorMode != MotorMode::Off || limitsEnabled)) {
		ImGui::Separator();
		ImGui::Text("Current Angle: %.1f deg", glm::degrees(currentAngle));
	}
}

void RevoluteConstraint::PostIterationClamp(std::vector<SolverRow>& rows, int idx, int iteration) {
	SolverRow& r = rows[idx];
	r.lambda = glm::clamp(r.lambda, r.minLambda, r.maxLambda);
}

void RevoluteConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
	if (objectA.obj == nullptr || objectB.obj == nullptr) return;

	glm::vec3 globalPointA = (objectA.pm != nullptr)
		? *objectA.position
		: glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));
	glm::vec3 globalPointB = (objectB.pm != nullptr)
		? *objectB.position
		: glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

	glm::vec3 rA = globalPointA - *objectA.position;
	glm::vec3 rB = globalPointB - *objectB.position;

	float invMassA = objectA.invMass ? *objectA.invMass : 0.0f;
	float invMassB = objectB.invMass ? *objectB.invMass : 0.0f;
	float invInertiaA = objectA.invInertia ? *objectA.invInertia : 0.0f;
	float invInertiaB = objectB.invInertia ? *objectB.invInertia : 0.0f;

	JacobianRow jacobianX = JacobianRow();
	SolverRow rowX = SolverRow();
	jacobianX.linearA = glm::vec3(1.0f, 0.0f, 0.0f);
	jacobianX.linearB = glm::vec3(-1.0f, 0.0f, 0.0f);
	jacobianX.angularA = -rA.y;
	jacobianX.angularB = rB.y;
	rowX.jacobian = jacobianX;

	JacobianRow jacobianY = JacobianRow();
	SolverRow rowY = SolverRow();
	jacobianY.linearA = glm::vec3(0.0f, 1.0f, 0.0f);
	jacobianY.linearB = glm::vec3(0.0f, -1.0f, 0.0f);
	jacobianY.angularA = rA.x;
	jacobianY.angularB = -rB.x;
	rowY.jacobian = jacobianY;

	float kX = invMassA * glm::length2(jacobianX.linearA) + invInertiaA * jacobianX.angularA * jacobianX.angularA +
		invMassB * glm::length2(jacobianX.linearB) + invInertiaB * jacobianX.angularB * jacobianX.angularB;
	float kY = invMassA * glm::length2(jacobianY.linearA) + invInertiaA * jacobianY.angularA * jacobianY.angularA +
		invMassB * glm::length2(jacobianY.linearB) + invInertiaB * jacobianY.angularB * jacobianY.angularB;

	rowX.effectiveMass = (kX > 0.0f) ? 1.0f / kX : 0.0f;
	rowY.effectiveMass = (kY > 0.0f) ? 1.0f / kY : 0.0f;

	glm::vec3 positionError = globalPointB - globalPointA;
	rowX.bias = (beta / delta) * positionError.x;
	rowY.bias = (beta / delta) * positionError.y;

	for (SolverRow* r : { &rowX, &rowY }) {
		r->maxLambda = INFINITY;
		r->minLambda = -INFINITY;
		r->objectA = objectA;
		r->objectB = objectB;
		r->parentConstraint = this;
	}
	rows.push_back(rowX);
	rows.push_back(rowY);

	bool needAngle = (motorMode != MotorMode::Off) || limitsEnabled;
	if (!needAngle) return;

	float rotA = objectA.rotation ? *objectA.rotation : 0.0f;
	float rotB = objectB.rotation ? *objectB.rotation : 0.0f;
	float rawAngle = rotB - rotA;

	const float twoPi = 2.0f * (float)std::numbers::pi;
	if (!angleInitialised) {
		referenceAngle = rawAngle;
		lastRawAngle = rawAngle;
		currentAngle = 0.0f;
		angleInitialised = true;
	}
	else {
		float d = rawAngle - lastRawAngle;
		d = d - twoPi * std::round(d / twoPi);   
		currentAngle += d;
		lastRawAngle = rawAngle;
	}

	float kAng = invInertiaA + invInertiaB;
	if (kAng <= 0.0f) return;
	float effMassAng = 1.0f / kAng;

	JacobianRow angJ = JacobianRow();
	angJ.linearA = glm::vec3(0.0f);
	angJ.linearB = glm::vec3(0.0f);
	angJ.angularA = -1.0f;
	angJ.angularB = 1.0f;

	auto makeAngRow = [&]() {
		SolverRow r = SolverRow();
		r.jacobian = angJ;
		r.effectiveMass = effMassAng;
		r.objectA = objectA;
		r.objectB = objectB;
		r.parentConstraint = this;
		return r;
		};

	if (motorMode != MotorMode::Off) {
		SolverRow motor = makeAngRow();

		float desiredRelVel = 0.0f;
		if (motorMode == MotorMode::Velocity) {
			desiredRelVel = motorSpeed;
		}
		else {
			float error = targetAngle - currentAngle;
			desiredRelVel = glm::clamp(servoGain * error, -maxMotorSpeed, maxMotorSpeed);
			
			float maxStep = std::abs(error) / delta;
			desiredRelVel = glm::clamp(desiredRelVel, -maxStep, maxStep);
		}

		motor.bias = desiredRelVel;
		float maxImpulse = maxMotorTorque * delta;   
		motor.maxLambda = maxImpulse;
		motor.minLambda = -maxImpulse;
		rows.push_back(motor);
	}

	if (limitsEnabled) {
		float lowErr = lowerLimit - currentAngle;   
		SolverRow lower = makeAngRow();
		lower.bias = (lowErr > 0.0f) ? (beta / delta) * lowErr : lowErr / delta;
		lower.minLambda = 0.0f;
		lower.maxLambda = INFINITY;
		rows.push_back(lower);

		float upErr = upperLimit - currentAngle;    
		SolverRow upper = makeAngRow();
		upper.bias = (upErr < 0.0f) ? (beta / delta) * upErr : upErr / delta;
		upper.minLambda = -INFINITY;
		upper.maxLambda = 0.0f;
		rows.push_back(upper);
	}
}

void RevoluteConstraint::DrawConstraintGizmo() {
	return;
}

void RevoluteConstraint::Serialize(BinaryWriter& w) {
	Constraint::Serialize(w);
	w.Write(static_cast<int32_t>(motorMode));
	w.Write(motorSpeed);
	w.Write(targetAngle);
	w.Write(servoGain);
	w.Write(maxMotorSpeed);
	w.Write(maxMotorTorque);
	w.Write(limitsEnabled);
	w.Write(lowerLimit);
	w.Write(upperLimit);
}

void RevoluteConstraint::Deserialize(BinaryReader& r) {
	Constraint::Deserialize(r);
	motorMode = static_cast<MotorMode>(r.Read<int32_t>());
	motorSpeed = r.Read<float>();
	targetAngle = r.Read<float>();
	servoGain = r.Read<float>();
	maxMotorSpeed = r.Read<float>();
	maxMotorTorque = r.Read<float>();
	limitsEnabled = r.Read<bool>();
	lowerLimit = r.Read<float>();
	upperLimit = r.Read<float>();

	angleInitialised = false;   
	currentAngle = 0.0f;
}