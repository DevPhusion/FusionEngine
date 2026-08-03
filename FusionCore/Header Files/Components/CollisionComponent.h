#pragma once
#include "Component.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "../Core/Rendering/Shapes.h"
#include "../Core/Physics/Collision/BoundingCircle.h"
#include "../Core/Physics/Collision/CollisionLayerMask.h"
#include "../Core/Physics/Collision/BAHNode.h"

enum CollisionType {
	RigidVsRigid,
	RigidVsStatic,
	StaticVsStatic,
	RigidVsSoft,
	SoftVsSoft,
	FluidVsRigid,
	FluidVsSoft
};

struct CollisionEventData {
	CollisionType type;
	Object* self = nullptr;      
	Object* other = nullptr;     
	glm::vec3 point = glm::vec3(0.0f);
	glm::vec3 normal = glm::vec3(0.0f); 
	float penetration = 0.0f;
};

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

	bool isGrounded(float probeLength = 0.15f);
	
	int AddCollisionCallback(std::function<void(const CollisionEventData&)> callback);
	void RemoveCollisionCallback(int id);
	void NotifyCollision(const CollisionEventData& data);

	int AddCollisionEnterCallback(std::function<void(const CollisionEventData&)> callback);
	void RemoveCollisionEnterCallback(int id);
	void NotifyCollisionEnter(const CollisionEventData& data);

	int AddCollisionExitCallback(std::function<void(const CollisionEventData&)> callback);
	void RemoveCollisionExitCallback(int id);
	void NotifyCollisionExit(const CollisionEventData& data);

private:
	std::unordered_map<int, std::function<void(const CollisionEventData&)>> collisionCallbacks;
	std::unordered_map<int, std::function<void(const CollisionEventData&)>> collisionEnterCallbacks;
	std::unordered_map<int, std::function<void(const CollisionEventData&)>> collisionExitCallbacks;
	int physicsChangeEventCallbackID = -1;
	int nextCollisionEnterCallbackID = 0;
	int nextCollisionExitCallbackID = 0;
	int nextCollisionCallbackID = 0;
	int polygonEditCallbackID = -1;

	void RebuildFromShape(const std::vector<glm::vec3>& localVerts);
	void ApplyLiveShapeUpdate(const std::vector<glm::vec3>& verts);
};