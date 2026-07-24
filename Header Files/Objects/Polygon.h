#define GLM_ENABLE_EXPERIMENTAL
#pragma once
#include "../Core/Rendering/Shader.h"
#include "Object.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/VertexComponent.h"
#include "../Components/RigidBodyComponent.h"
#include "../Components/MouseInteractComponent.h"
#include "../Components/CollisionComponent.h"
#include "../Components/ConstraintComponent.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <string>


class Polygon : public Object
{
public:
	Polygon(std::vector<float> vertices, Shader shader, std::string texture_path);
	Polygon() = default;

	virtual void Process(float delta);
};