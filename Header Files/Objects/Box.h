#pragma once
#include "../Core/Rendering/Shader.h"
#include "Object.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/RigidBodyComponent.h"
#include "../Components/MouseInteractComponent.h"
#include "../Components/CollisionComponent.h"
#include "../Components/ConstraintComponent.h"

class Box : public Object
{
public:
	Box(Shader shader, std::string texture_path);
	Box() = default;

	virtual void Process(float delta);
};

