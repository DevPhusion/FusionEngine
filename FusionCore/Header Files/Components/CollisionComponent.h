#pragma once
#include "Component.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "../Core/Rendering/Shapes.h"
#include "../Core/Physics/Collision/BoundingCircle.h"
#include "../Core/Physics/Collision/CollisionLayerMask.h"
#include "../Core/Physics/Collision/BAHNode.h"

class CollisionComponent : public ComponentBase<CollisionComponent>
{
public:
	CollisionComponent(Object* parent);
	CollisionComponent() = default;

	BoundingCircle boundingCircle;
	BAHNode<BoundingCircle>* BAHnode;

	uint16_t collisionLayer = static_cast<uint16_t>(CollisionLayer::LAYER_1);
	uint16_t collisionMask = static_cast<uint16_t>(CollisionMask::LAYER_1);

	bool isStatic = true;

	int onTransformCallbackID = -1;

	Shape currentShape;
	Shape pendingShape; 

	std::vector<std::vector<float>> points; 
	std::vector<Edge> edges;               

	bool syncWithRenderComponent = false;
	int renderSyncCallbackID = -1;
	bool isAddVertex = false; 

	virtual void SetEnabled(bool enabled);
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
	virtual void PostLoad();

	void DrawLayerMaskUI(const char* label, uint16_t* layer);
	void calculateBoundingCircle();

	void SetShape(Shape shape);
	std::vector<glm::vec3> VerticesFromShape(Shape& shape);
	glm::vec3 GetCenter();
	float GetArea();
	void Draw();

	void SetCollisionLayer(uint16_t layer);
	void SetCollisionMask(uint16_t mask);

	void SetSyncWithRenderComponent(bool sync);
	void SyncFromRenderComponent();

private:
	void RebuildFromShape(const std::vector<glm::vec3>& localVerts);
	void ApplyLiveShapeUpdate(const std::vector<glm::vec3>& verts);
	int polygonEditCallbackID = -1;
};