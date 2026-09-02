#include "../../../../Header Files/Core/Physics/Collision/BoundingCircle.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"

BoundingCircle::BoundingCircle(const glm::vec3 center, float radius, uint16_t collisionLayer, uint16_t collisionMask) {
    this->center = center;
    this->radius = radius;
    this->collisionLayer = collisionLayer;
    this->collisionMask = collisionMask;
}

BoundingCircle::BoundingCircle(const BoundingCircle& one, const BoundingCircle& two) {
    float dist = glm::distance(one.center, two.center);

    radius = (dist + one.radius + two.radius) * 0.5f;

    if (one.radius >= dist + two.radius) {
        center = one.center;
        radius = one.radius;
        return;
    }
    if (two.radius >= dist + one.radius) {
        center = two.center;
        radius = two.radius;
        return;
    }

    if (dist > 0.0f) {
        center = one.center + (two.center - one.center) * ((radius - one.radius) / dist);
    }
    else {
        center = one.center;
    }

    collisionLayer = 0xFFFF;
    collisionMask = 0xFFFF;
}

float BoundingCircle::getGrowth(const BoundingCircle& other) const {
    float dist = glm::distance(center, other.center);
    float newRadius = (dist + radius + other.radius) * 0.5f;
    return newRadius - radius;
}

bool BoundingCircle::overlaps(const BoundingCircle* other) const {
    if (!layerOverlap(collisionLayer, collisionMask, other->collisionLayer, other->collisionMask)) {
        return false;
    }

    float distanceSquared = distance2(center, other->center);
    return distanceSquared <= (radius + other->radius) * (radius + other->radius);
}

void BoundingCircle::DebugDraw() const {
    Renderer::getInstance().DrawCircle(
        center,
        radius,
        glm::vec4(0.9f, 0.5f, 0.2f, 0.85f),   
        32,                                   
        3.0f,                                  
        false);                               
}