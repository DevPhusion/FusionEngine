#pragma once
#include <string>
#include <variant>
#include <glm/glm.hpp>

using ExportedValue = std::variant<std::string, int, float, bool, glm::vec2, glm::vec3>;

struct ExportedProperty {
	std::string name;
	ExportedValue value;
};