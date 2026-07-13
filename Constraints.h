#pragma once
#include "DistanceConstraint.h"
#include "PrismaticConstraint.h"
#include "RevoluteConstraint.h"
#include "SpringConstraint.h"
#include "WeldConstraint.h"

std::shared_ptr<Constraint> CreateConstraintFromName(std::string name) {
	if (name == "Distance Constraint") {
		return std::make_shared<DistanceConstraint>(PhysicsBody(), PhysicsBody(), glm::vec3(0), glm::vec3(0), 0);
	}
	if (name == "Spring Constraint") {
		return std::make_shared<SpringConstraint>(PhysicsBody(), PhysicsBody(), glm::vec3(0), glm::vec3(0), 0);
	}
	if (name == "Revolute Constraint") {
		return std::make_shared<RevoluteConstraint>(PhysicsBody(), PhysicsBody(), glm::vec3(0), glm::vec3(0));
	}
	if (name == "Weld Constraint") {
		return std::make_shared<WeldConstraint>(PhysicsBody(), PhysicsBody(), glm::vec3(0), glm::vec3(0), 0);
	}
	if (name == "Prismatic Constraint") {
		return std::make_shared<PrismaticConstraint>(PhysicsBody(), PhysicsBody(), glm::vec3(0), glm::vec3(0), glm::vec3(0));
	}

	return nullptr;
}