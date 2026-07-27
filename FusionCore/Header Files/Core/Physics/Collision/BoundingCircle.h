#pragma once
#include "../../../Objects/Object.h"
#include "CollisionLayerMask.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp> 

struct BoundingCircle {
public:
	BoundingCircle() = default;
	BoundingCircle(const glm::vec3 center, float radius, uint16_t collisionLayer, uint16_t collisionMask);
	//Bounding sphere to bound two smaller sphere
	BoundingCircle(const BoundingCircle& one, const BoundingCircle& two);

	glm::vec3 center;
	float radius;

	uint16_t collisionLayer = static_cast<uint16_t>(CollisionLayer::LAYER_1);
	uint16_t collisionMask = static_cast<uint16_t>(CollisionMask::LAYER_1);

	bool overlaps(const BoundingCircle* other) const;
	float getSize() const {
		return radius;
	}

	float getGrowth(const BoundingCircle& other) const;
};

