#pragma once
#include "Component.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "BoundingCircle.h"
#include "CollisionLayerMask.h"
#include "BAHNode.h"

class CollisionComponent : public ComponentBase<CollisionComponent>
{
public:
	CollisionComponent(Object* parent);
	CollisionComponent() = default;

	BoundingCircle boundingCircle;
	BAHNode<BoundingCircle>* BAHnode;

	uint16_t collisionLayer = static_cast<uint16_t>(CollisionLayer::LAYER_1);
	uint16_t collisionMask = static_cast<uint16_t>(CollisionMask::LAYER_1);

	virtual void SetEnabled(bool enabled);
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void DrawLayerMaskUI(const char* label, uint16_t* layer);
	void calculateBoundingCircle();
private:
	int onTransformCallbackID = -1;
};

