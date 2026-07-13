#pragma once
#include "Object.h"
#include "Shader.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include <memory>
class VertexPoint:public Object
{

public:
	VertexPoint(float x, float y, Shader shader);
	VertexPoint() = default;
	virtual void Process(float delta);

	float x;
	float y;

	std::unique_ptr<VertexPoint> CloneVertex();
	void SerializeVertex(BinaryWriter& w);
	void DeserializeVertex(BinaryReader& r);
	void UpdatePosition(float x, float y);
};