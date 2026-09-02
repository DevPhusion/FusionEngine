#include "../../../../Header Files/Core/Physics/Collision/BoundingBox.h"
#include "../../../../Header Files/Core/Rendering/Renderer.h"

BoundingBox::BoundingBox(const glm::vec3& center, const glm::vec3& halfExtents, uint16_t collisionLayer, uint16_t collisionMask) {
	this->center = center;
	this->halfExtents = halfExtents;
	this->collisionLayer = collisionLayer;
	this->collisionMask = collisionMask;
}

BoundingBox::BoundingBox(const BoundingBox& one, const BoundingBox& two) {
	glm::vec3 mn = glm::min(one.Min(), two.Min());
	glm::vec3 mx = glm::max(one.Max(), two.Max());
	center = (mn + mx) * 0.5f;
	halfExtents = (mx - mn) * 0.5f;

	collisionLayer = 0xFFFF;
	collisionMask = 0xFFFF;
}

bool BoundingBox::overlaps(const BoundingBox* other) const {
	if (!layerOverlap(collisionLayer, collisionMask, other->collisionLayer, other->collisionMask)) {
		return false;
	}

	glm::vec3 aMin = Min(), aMax = Max();
	glm::vec3 bMin = other->Min(), bMax = other->Max();

	return (aMin.x <= bMax.x && aMax.x >= bMin.x) &&
		(aMin.y <= bMax.y && aMax.y >= bMin.y);
}

float BoundingBox::getGrowth(const BoundingBox& other) const {
	glm::vec3 mn = glm::min(Min(), other.Min());
	glm::vec3 mx = glm::max(Max(), other.Max());
	glm::vec3 newHalf = (mx - mn) * 0.5f;

	float newMetric = newHalf.x + newHalf.y;
	float metric = halfExtents.x + halfExtents.y;
	return newMetric - metric;
}

void BoundingBox::DebugDraw() const {
	std::vector<glm::vec3> corners = {
		center + glm::vec3(-halfExtents.x, -halfExtents.y, 0.0f),
		center + glm::vec3(halfExtents.x, -halfExtents.y, 0.0f),
		center + glm::vec3(halfExtents.x,  halfExtents.y, 0.0f),
		center + glm::vec3(-halfExtents.x,  halfExtents.y, 0.0f),
	};

	Renderer::getInstance().DrawFilledPolygon(
		corners,
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),        
		glm::vec4(0.9f, 0.5f, 0.2f, 0.85f),       
		3.0f);
}

BoundingBox BoundingBox::FromPoints(const std::vector<glm::vec3>& worldPoints, uint16_t collisionLayer, uint16_t collisionMask) {
	if (worldPoints.empty()) return BoundingBox(glm::vec3(0.0f), glm::vec3(0.0f), collisionLayer, collisionMask);

	glm::vec3 mn(INFINITY), mx(-INFINITY);
	for (auto& p : worldPoints) {
		mn = glm::min(mn, p);
		mx = glm::max(mx, p);
	}
	return BoundingBox((mn + mx) * 0.5f, (mx - mn) * 0.5f, collisionLayer, collisionMask);
}