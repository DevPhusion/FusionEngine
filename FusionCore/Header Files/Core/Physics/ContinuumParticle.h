#pragma once
#include "../../Objects/Object.h"
#include <variant>

struct FluidParticle {
    Object* parent;
    glm::vec3 position;
    glm::vec3 predictedPosition;
    glm::vec3 velocity;
    float collisionRadius;
    float restDensity;
    float density;
    float viscosity;

    float invMass;
    float mass;

    float lambda;
    float smoothingRadius;

    float epsilon;
    float vorticityEps;

    float poly6Coeff = 0.0f;
    float spikyCoeff = 0.0f;

    uint16_t collisionLayer = 0xFFFF;
    uint16_t collisionMask = 0xFFFF;
};

struct GasParticle {
    Object* parent;
    glm::vec3 position;
    glm::vec3 predictedPosition;
    glm::vec3 velocity;
    float collisionRadius;
    float restDensity;
    float density;
    float viscosity;

    float stiffness;
    float gamma;
    float compliance;

    float temperature;
	float ambientTemperature;
    float dissipationRate;

    float invMass;
    float mass;

    float lambda;
    float smoothingRadius;

    float epsilon;
    float vorticityEps;

    float poly6Coeff = 0.0f;
    float spikyCoeff = 0.0f;

    uint16_t collisionLayer = 0xFFFF;
    uint16_t collisionMask = 0xFFFF;
};

using ContinuumParticle = std::variant<FluidParticle*, GasParticle*>;