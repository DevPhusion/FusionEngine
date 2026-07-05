#pragma once
#include "Object.h"

struct PhysicsBody {
    Object* obj = nullptr; // set if is rigidbody
    void* pm = nullptr; // set if is soft body point mass
    glm::mat4* transformMatrix;

    glm::vec3* position = nullptr;
    glm::vec3* prevPos = nullptr;
    float* rotation = nullptr;
    float* invMass = nullptr;
    float* invInertia = nullptr;
    glm::vec3* velocity = nullptr;
    float* angularVelocity = nullptr;
};