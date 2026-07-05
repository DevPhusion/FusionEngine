#include "XPBDContactConstraint.h"

XPBDContactConstraint::XPBDContactConstraint(PhysicsBody objA, PhysicsBody objB, glm::vec3 normal, float separation, float compliance, float staticFriction, float dynamicFriction) {
	this->objA = objA;
	this->objB = objB;
	this->normal = normal;
	this->separation = separation;
	this->compliance = compliance;
	this->staticFriction = staticFriction;
	this->dynamicFriction = dynamicFriction;
    this->isTemporary = true;
}

void XPBDContactConstraint::SolvePosition(float delta) {
    if (objA.invMass == nullptr || objB.invMass == nullptr) return;
    if (delta <= 0.0f) return;

    // Normal constraint

    float slop = 0.005f;  
    float C_n = glm::dot(normal, (*objA.position - *objB.position)) - separation;
    if (C_n >= -slop) return;
    if (C_n >= 0.0f) return;   

    glm::vec3 gradA_n = normal;
    glm::vec3 gradB_n = -normal;

    float wSum_n = (*objA.invMass) + (*objB.invMass);
    if (wSum_n < 1e-9f) return;

    float deltaLambda_n;
    if (compliance <= 0.0f) {
        deltaLambda_n = -C_n / wSum_n;
    }
    else {
        float alphaTilde_n = compliance / (delta * delta);
        deltaLambda_n = (-C_n - alphaTilde_n * lambda[0]) / (wSum_n + alphaTilde_n);
    }

    float lambdaNew_n = lambda[0] + deltaLambda_n;
    if (lambdaNew_n < 0.0f) {
        deltaLambda_n = -lambda[0];
        lambda[0] = 0.0f;
    }
    else {
        lambda[0] = lambdaNew_n;
    }

    *objA.position += (*objA.invMass) * gradA_n * deltaLambda_n;
    *objB.position += (*objB.invMass) * gradB_n * deltaLambda_n;

    // Friction
    glm::vec3 tangent = glm::vec3(-normal.y, normal.x, 0.0f);
    float C_t = glm::dot(tangent, (*objA.position - *objA.prevPos) - (*objB.position - *objB.prevPos));

    glm::vec3 gradA_t = tangent;
    glm::vec3 gradB_t = -tangent;

    float wSum_t = (*objA.invMass) + (*objB.invMass);
    if (wSum_t < 1e-9f) return;

    float lambdaOld_t = lambda[1];
    float deltaLambdaRaw_t = -C_t / wSum_t;
    float lambdaNew_t = lambdaOld_t + deltaLambdaRaw_t;
    float maxStatic = staticFriction * lambda[0];

    if (fabsf(lambdaNew_t) <= maxStatic) {
        lambda[1] = lambdaNew_t;
    }
    else {
        float maxDynamic = dynamicFriction * lambda[0];
        lambda[1] = glm::clamp(lambdaNew_t, -maxDynamic, maxDynamic);
    }
    float deltaLambda_t = lambda[1] - lambdaOld_t;

    *objA.position += (*objA.invMass) * gradA_t * deltaLambda_t;
    *objB.position += (*objB.invMass) * gradB_t * deltaLambda_t;
}
