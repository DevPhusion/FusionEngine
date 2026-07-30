#pragma once
#include "Object.h"
#include "../Core/Rendering/Shader.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include <memory>
class VertexPoint:public Object
{

public:
	VertexPoint(float x, float y, Shader shader);
	VertexPoint() = default;

	float x;
	float y;

	std::unique_ptr<VertexPoint> CloneVertex();
	void SerializeVertex(BinaryWriter& w);
	void DeserializeVertex(BinaryReader& r);
	void UpdatePosition(float x, float y);
};