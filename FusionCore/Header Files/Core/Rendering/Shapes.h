#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <variant>

struct Edge {
	glm::vec3 start;
	glm::vec3 end;
};

struct PolygonShape {
	std::vector<float> vertices;
};

struct RectangleShape {
	glm::vec3 center;
	float width;
	float height;
};

struct CircleShape {
	glm::vec3 center;
	float radius;
	int segments = 30;
};

using Shape = std::variant<PolygonShape, RectangleShape, CircleShape>;