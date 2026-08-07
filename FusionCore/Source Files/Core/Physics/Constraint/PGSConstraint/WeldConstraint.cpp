#include "../../../../../Header Files/Core/Physics/Constraint/PGSConstraint/WeldConstraint.h"

WeldConstraint::WeldConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, float angularOffset) :
	Constraint(objectA, objectB, attachPointA, attachPointB) {
	this->angularOffset = angularOffset;
	this->Name = "Weld Constraint";
}

void WeldConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
    if (objectA.obj == nullptr || objectB.obj == nullptr) {
        return;
    }

    glm::vec3 globalPointA = (objectA.pm != nullptr)
        ? *objectA.position
        : glm::vec3(*objectA.transformMatrix * glm::vec4(attachPointA, 1));

    glm::vec3 globalPointB = (objectB.pm != nullptr)
        ? *objectB.position
        : glm::vec3(*objectB.transformMatrix * glm::vec4(attachPointB, 1));

    glm::vec3 rA = globalPointA - *objectA.position;
    glm::vec3 rB = globalPointB - *objectB.position;

    JacobianRow jacobianX, jacobianY, jacobianTheta;
    SolverRow rowX, rowY, rowTheta;

    jacobianX.linearA = glm::vec3(1.0f, 0.0f, 0.0f);
    jacobianX.linearB = glm::vec3(-1.0f, 0.0f, 0.0f);
    jacobianX.angularA = -rA.y;
    jacobianX.angularB = rB.y;

    jacobianY.linearA = glm::vec3(0.0f, 1.0f, 0.0f);
    jacobianY.linearB = glm::vec3(0.0f, -1.0f, 0.0f);
    jacobianY.angularA = rA.x;
    jacobianY.angularB = -rB.x;

    jacobianTheta.linearA = glm::vec3(0);
    jacobianTheta.linearB = glm::vec3(0);
    jacobianTheta.angularA = 1;
    jacobianTheta.angularB = -1;

    float invMassA = objectA.invMass ? *objectA.invMass : 0.0f;
    float invMassB = objectB.invMass ? *objectB.invMass : 0.0f;
    float invInertiaA = objectA.invInertia ? *objectA.invInertia : 0.0f;
    float invInertiaB = objectB.invInertia ? *objectB.invInertia : 0.0f;
    float thetaA = *objectA.rotation;
    float thetaB = *objectB.rotation;

    float kX = invMassA * glm::length2(jacobianX.linearA) + invInertiaA * jacobianX.angularA * jacobianX.angularA +
        invMassB * glm::length2(jacobianX.linearB) + invInertiaB * jacobianX.angularB * jacobianX.angularB;

    float kY = invMassA * glm::length2(jacobianY.linearA) + invInertiaA * jacobianY.angularA * jacobianY.angularA +
        invMassB * glm::length2(jacobianY.linearB) + invInertiaB * jacobianY.angularB * jacobianY.angularB;

    float kTheta = invInertiaA + invInertiaB;

    float softnessCFM = 0.0f;
    float finalBeta = beta;

    glm::vec3 positionError = globalPointB - globalPointA;
    float angleError = thetaB - thetaA - angularOffset;

    float biasX = (finalBeta / delta) * positionError.x;
    float biasY = (finalBeta / delta) * positionError.y;
    float biasTheta = (finalBeta / delta) * angleError;

    rowX.jacobian = jacobianX;
    rowX.effectiveMass = (kX > 0.0f) ? 1.0f / kX : 0.0f;
    rowX.bias = biasX;
    rowX.maxLambda = INFINITY;
    rowX.minLambda = -INFINITY;
    rowX.objectA = objectA;
    rowX.objectB = objectB;
    rowX.parentConstraint = this;
    rowX.lambda = cacheLambda[0];
    rowX.softnessCFM = softnessCFM;

    rowY.jacobian = jacobianY;
    rowY.effectiveMass = (kY > 0.0f) ? 1.0f / kY : 0.0f;
    rowY.bias = biasY;
    rowY.maxLambda = INFINITY;
    rowY.minLambda = -INFINITY;
    rowY.objectA = objectA;
    rowY.objectB = objectB;
    rowY.parentConstraint = this;
    rowY.lambda = cacheLambda[1];
    rowY.softnessCFM = softnessCFM;

    rowTheta.jacobian = jacobianTheta;
    rowTheta.effectiveMass = (kTheta > 0.0f) ? 1.0f / kTheta : 0.0f;
    rowTheta.bias = biasTheta;
    rowTheta.maxLambda = INFINITY;
    rowTheta.minLambda = -INFINITY;
    rowTheta.objectA = objectA;
    rowTheta.objectB = objectB;
    rowTheta.parentConstraint = this;
    rowTheta.lambda = cacheLambda[2];
    rowTheta.softnessCFM = softnessCFM;

    xRowOffset = static_cast<int>(rows.size());
    rows.push_back(rowX);
    yRowOffset = static_cast<int>(rows.size());
    rows.push_back(rowY);
    thetaRowOffset = static_cast<int>(rows.size());
    rows.push_back(rowTheta);
}

void WeldConstraint::PostSolve(std::vector<SolverRow>& allRows) {
	cacheLambda[0] = allRows[xRowOffset].lambda;
	cacheLambda[1] = allRows[yRowOffset].lambda;
	cacheLambda[2] = allRows[thetaRowOffset].lambda;
}

void WeldConstraint::SetObjectA(PhysicsBody obj) {
	Constraint::SetObjectA(obj);

	if (objectA.obj != nullptr && objectB.obj != nullptr) {
		float thetaA = objectA.obj->GetComponent<TransformComponent>()->rotation;
		float thetaB = objectB.obj->GetComponent<TransformComponent>()->rotation;
		this->angularOffset = thetaB - thetaA;
	}
}
void WeldConstraint::SetObjectB(PhysicsBody obj) {
	Constraint::SetObjectB(obj);

	if (objectA.obj != nullptr && objectB.obj != nullptr) {
		float thetaA = objectA.obj->GetComponent<TransformComponent>()->rotation;
		float thetaB = objectB.obj->GetComponent<TransformComponent>()->rotation;
		this->angularOffset = thetaB - thetaA;
	}
}

void WeldConstraint::ProcessInspectorUI(Object* parent) {
	Constraint::ProcessInspectorUI(parent);

	if (objectA.obj && objectB.obj) {
	    ImGui::Text("Locked angle ");
	    ImGui::BeginDisabled();
	    ImGui::SliderAngle("##Locked angle", &angularOffset);
	    ImGui::EndDisabled();
        if (ImGui::Button("Re-lock angle")) {
		    float thetaA = objectA.obj->GetComponent<TransformComponent>()->rotation;
		    float thetaB = objectB.obj->GetComponent<TransformComponent>()->rotation;
            angularOffset = thetaB - thetaA;
            EngineManager::getInstance().EngineChangeEvent();
        }
	}
}

std::shared_ptr<Constraint> WeldConstraint::Clone() {
    std::shared_ptr<WeldConstraint> constraint = std::make_shared<WeldConstraint>(PhysicsBody(), PhysicsBody(), attachPointA, attachPointB, angularOffset);
    constraint->CopyBaseFieldsFrom(this);
    return constraint;
}

void WeldConstraint::Serialize(BinaryWriter& w) {
    Constraint::Serialize(w);
    w.Write(angularOffset);
}

void WeldConstraint::Deserialize(BinaryReader& r) {
    Constraint::Deserialize(r);
    angularOffset = r.Read<float>();
}