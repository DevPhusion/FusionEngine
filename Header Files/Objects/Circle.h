#pragma once
#include "../Core/Rendering/Shader.h"
#include "Object.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/RigidBodyComponent.h"
#include "../Components/MouseInteractComponent.h"
#include "../Components/CollisionComponent.h"
#include "../Components/ConstraintComponent.h"

class Circle : public Object
{
public:
	Circle(Shader shader, std::string texture_path);
	Circle() = default;

	virtual void Process(float delta);
};

