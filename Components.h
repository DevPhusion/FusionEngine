#pragma once
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "VertexComponent.h"
#include "MouseInteractComponent.h"
#include "CollisionComponent.h"
#include "ConstraintComponent.h"
#include "RigidBodyComponent.h"
#include "SoftBodyComponent.h"

std::unique_ptr<Component> CreateComponentFromName(Object* parent, std::string name) {
	if (name == "Render Component") {
		return std::make_unique<RenderComponent>(parent, std::vector<float> {}, parent->shader, "");
	}
	if (name == "Transform Component") {
		return std::make_unique<TransformComponent>(parent, parent->shader, glm::vec3(0));
	}
	if (name == "Vertex Component") {
		return std::make_unique<VertexComponent>(parent);
	}
	if (name == "Mouse Interact Component") {
		return std::make_unique<MouseInteractComponent>(parent, false);
	}
	if (name == "Collision Component") {
		return std::make_unique<CollisionComponent>(parent);
	}
	if (name == "Constraint Component") {
		return std::make_unique<ConstraintComponent>(parent);
	}
	if (name == "Rigid Body Component") {
		return std::make_unique<RigidBodyComponent>(parent);
	}
	if (name == "Soft Body Component") {
		return std::make_unique<SoftBodyComponent>(parent);
	}

	return nullptr;
}