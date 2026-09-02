#pragma once
#include "../../../Objects/Object.h"
#include "CollisionLayerMask.h"

struct BoundingBox {
	BoundingBox() = default;
	BoundingBox(const glm::vec3& center, const glm::vec3& halfExtents, uint16_t collisionLayer, uint16_t collisionMask);
	BoundingBox(const BoundingBox& one, const BoundingBox& two);

	glm::vec3 center = glm::vec3(0.0f);
	glm::vec3 halfExtents = glm::vec3(0.0f);

	uint16_t collisionLayer = static_cast<uint16_t>(CollisionLayer::LAYER_1);
	uint16_t collisionMask = static_cast<uint16_t>(CollisionMask::LAYER_1);

	glm::vec3 Min() const { return center - halfExtents; }
	glm::vec3 Max() const { return center + halfExtents; }

	bool overlaps(const BoundingBox* other) const;

	float getSize() const { return halfExtents.x + halfExtents.y; }
	float getGrowth(const BoundingBox& other) const;

	void DebugDraw() const;

	static BoundingBox FromPoints(const std::vector<glm::vec3>& worldPoints, uint16_t collisionLayer, uint16_t collisionMask);
};