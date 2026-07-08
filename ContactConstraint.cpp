#include "ContactConstraint.h"
#include "PhysicsEngine.h"

ContactConstraint::ContactConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, ContactID id,
    glm::vec3 normal, float penetration, float restitution, float staticFriction, float dynamicFriction)
 {
    this->objectA = objectA;
    this->objectB = objectB;
    this->attachPointA = attachPointA;
    this->attachPointB = attachPointB;
    this->contactId = id;
    this->normal = normal;
    this->penetration = penetration;
    this->restitution = restitution;
    this->staticFriction = staticFriction;
    this->dynamicFriction = dynamicFriction;
    this->weightA = 1.0f;
    this->weightB = 1.0f;
    this->isTemporary = true;
}

ContactConstraint::ContactConstraint(PhysicsBody objectA, PhysicsBody objectB, glm::vec3 attachPointA, glm::vec3 attachPointB, ContactID id,
    glm::vec3 normal, float penetration, float restitution, float staticFriction, float dynamicFriction,
    float weightA, float weightB)
{
    this->objectA = objectA;
    this->objectB = objectB;
    this->attachPointA = attachPointA;
    this->attachPointB = attachPointB;
    this->contactId = id;
    this->normal = normal;
    this->penetration = penetration;
    this->restitution = restitution;
    this->staticFriction = staticFriction;
    this->dynamicFriction = dynamicFriction;
    this->isTemporary = true;
    this->weightA = weightA;
    this->weightB = weightB;
}

void ContactConstraint::Prepare(std::vector<SolverRow>& rows, float delta) {
    glm::vec3 rA = attachPointA - *objectA.position;
    glm::vec3 rB = attachPointB - *objectB.position;

    this->normalRowOffsetIndex = static_cast<int>(rows.size());

    JacobianRow nJac;
    nJac.linearA = glm::vec3(normal.x, normal.y, 0.0f) * weightA;
    nJac.linearB = -glm::vec3(normal.x, normal.y, 0.0f) * weightB;
    nJac.angularA = (rA.x * normal.y - rA.y * normal.x) * weightA;
    nJac.angularB = -(rB.x * normal.y - rB.y * normal.x) * weightB;

    float invMassA = objectA.invMass ? *objectA.invMass : 0.0f;
    float invMassB = objectB.invMass ? *objectB.invMass : 0.0f;
    float invInertiaA = objectA.invInertia ? *objectA.invInertia : 0.0f;
    float invInertiaB = objectB.invInertia ? *objectB.invInertia : 0.0f;

    float kN = invMassA * glm::length2(nJac.linearA) + invInertiaA * (nJac.angularA * nJac.angularA)
        + invMassB * glm::length2(nJac.linearB) + invInertiaB * (nJac.angularB * nJac.angularB);
    float effectiveMassNormal = (kN > 0.0f) ? 1.0f / kN : 0.0f;

    float relVel = 0.0f;
    if (objectA.velocity && objectA.angularVelocity) relVel += glm::dot(nJac.linearA, *objectA.velocity) + nJac.angularA * *objectA.angularVelocity;
    if (objectB.velocity && objectB.angularVelocity) relVel += glm::dot(nJac.linearB, *objectB.velocity) + nJac.angularB * *objectB.angularVelocity;

    const float beta = 0.2f;  
    const float slop = 0.005f; 
    const float maxBias = 4.0f;   

    float weightScale = std::min(weightA, weightB);
    float posBias = (beta / delta) * std::max(0.0f, penetration - slop) * weightScale;
    posBias = std::min(posBias, maxBias);

    float restitutionBias = 0.0f;
    if (relVel < -bounceThreshold) {
        restitutionBias = -restitution * relVel;
    }

    float combinedBias = std::max(posBias, restitutionBias);

    SolverRow nRow;
    nRow.jacobian = nJac;
    nRow.effectiveMass = effectiveMassNormal;
    nRow.bias = combinedBias;
    nRow.minLambda = 0.0f;
    nRow.maxLambda = INFINITY;
    nRow.lambda = cacheLambda; 
    nRow.parentConstraint = this;
    nRow.objectA = objectA;
    nRow.objectB = objectB;
    rows.push_back(nRow);

    this->frictionRowOffsetIndex = static_cast<int>(rows.size());

    glm::vec3 tangent = glm::vec3(-normal.y, normal.x, 0.0f);
    JacobianRow fJac;
    fJac.linearA = glm::vec3(tangent.x, tangent.y, 0.0f);
    fJac.linearB = -glm::vec3(tangent.x, tangent.y, 0.0f);
    fJac.angularA = (rA.x * tangent.y - rA.y * tangent.x);
    fJac.angularB = -(rB.x * tangent.y - rB.y * tangent.x);

    float kF = invMassA * glm::length2(fJac.linearA) + invInertiaA * (fJac.angularA * fJac.angularA)
        + invMassB * glm::length2(fJac.linearB) + invInertiaB * (fJac.angularB * fJac.angularB);

    SolverRow fRow;
    fRow.jacobian = fJac;
    fRow.effectiveMass = (kF > 0.0f) ? 1.0f / kF : 0.0f;
    fRow.bias = 0.0f;
    fRow.minLambda = -INFINITY;
    fRow.maxLambda = INFINITY;
    fRow.lambda = 0.0f;
    fRow.parentConstraint = this;
    fRow.objectA = objectA;
    fRow.objectB = objectB;
    rows.push_back(fRow);
}

void ContactConstraint::WarmStartSoftBody() {
    if (objectA.obj && objectB.obj) return;

    if (penetration > 0.0f) {
        if (!objectA.obj && objectA.position != nullptr) {
            *objectA.position += normal * penetration;
        }
        else if (!objectB.obj && objectB.position != nullptr) {
            *objectB.position -= normal * penetration;
        }
    }
}

void ContactConstraint::PostIterationClamp(std::vector<SolverRow>& allRows, int myRowIndex, int velocityIteration) {
    if (myRowIndex == normalRowOffsetIndex) {
        allRows[myRowIndex].lambda = std::max(0.0f, allRows[myRowIndex].lambda);
        return;
    }

    if (myRowIndex < frictionRowOffsetIndex) {
        allRows[myRowIndex].lambda = std::max(0.0f, allRows[myRowIndex].lambda);
        return;
    }

    float normalLambda = allRows[normalRowOffsetIndex].lambda;
    float maxStatic = staticFriction * normalLambda;
    float maxDynamic = dynamicFriction * normalLambda;
    float f = allRows[myRowIndex].lambda;

    if (std::abs(f) <= maxStatic) {
        allRows[myRowIndex].lambda = glm::clamp(f, -maxStatic, maxStatic);
    }
    else {
        allRows[myRowIndex].lambda = glm::clamp(f, -maxDynamic, maxDynamic);
    }
}

void ContactConstraint::PostSolve(std::vector<SolverRow>& allRows) {
    this->cacheLambda = allRows[this->normalRowOffsetIndex].lambda;
}