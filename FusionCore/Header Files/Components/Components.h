#pragma once
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "MouseInteractComponent.h"
#include "CollisionComponent.h"
#include "ConstraintComponent.h"
#include "RigidBodyComponent.h"
#include "SoftBodyComponent.h"
#include "FractureComponent.h"
#include "FluidComponent.h"
#include "ScriptComponent.h"
#include "CameraComponent.h"
#include "EditorRenderComponent.h"
#include "AgentComponent.h"

namespace {
	std::unique_ptr<Component> CreateComponentFromName(Object* parent, std::string name) {
		if (name == "Render Component") {
			return std::make_unique<RenderComponent>(parent, std::vector<float> {}, parent->shader, "");
		}
		if (name == "Transform Component") {
			return std::make_unique<TransformComponent>(parent, parent->shader, glm::vec3(0));
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
		if (name == "Fracture Component") {
			return std::make_unique<FractureComponent>(parent);
		}
		if (name == "Fluid Component") {
			return std::make_unique<FluidComponent>(parent);
		}
		if (name == "Camera Component") {
			return std::make_unique<CameraComponent>(parent);
		}
		if (name == "Editor Render Component") {
			return std::make_unique<EditorRenderComponent>(parent, parent->shader);
		}
		if (name == "Script Component") {
			return std::make_unique<ScriptComponent>(parent, "");
		}
		if (name == "Agent Component") {
			return std::make_unique<AgentComponent>(parent);
		}

		return nullptr;
	}
}
