#pragma once
#include "Component.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "../Core/Rendering/Shapes.h"
#include "../Core/Physics/Collision/BoundingCircle.h"
#include "../Core/Physics/Collision/CollisionLayerMask.h"
#include "../Core/Physics/Collision/BAHNode.h"
#include "../Core/Physics/Collision/BoundingBox.h"
#include "../Core/Physics/Collision/BroadPhaseHandle.h"
#include <string>
#include <vector>

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
	int selfShapeId = -1;
	int otherShapeId = -1;
	glm::vec3 point = glm::vec3(0.0f);
	glm::vec3 normal = glm::vec3(0.0f);
	float penetration = 0.0f;
};

struct CollisionShapeEntry {
	int id = -1;
	std::string name = "Shape";

	Shape currentShape;
	Shape pendingShape;

	bool syncWithRenderComponent = false;
	int renderSyncCallbackID = -1;

	std::vector<std::vector<float>> points;
	std::vector<Edge> edges;

	bool isAddVertex = false;
	int polygonEditCallbackID = -1;

	BoundingCircle boundingCircle;
	BoundingBox boundingBox;          
	BroadPhaseHandle BAHnode;        
};

class CollisionComponent : public ComponentBase<CollisionComponent>
{
public:
	CollisionComponent(Object* parent);
	CollisionComponent() = default;

	Shape resolutionShape;
	std::vector<std::vector<float>> points;
	std::vector<Edge> edges;

	uint16_t collisionLayer = static_cast<uint16_t>(CollisionLayer::LAYER_1);
	uint16_t collisionMask = static_cast<uint16_t>(CollisionMask::LAYER_1);

	bool isStatic = true;

	int onTransformCallbackID = -1;

	std::vector<CollisionShapeEntry> shapes;
	int resolutionShapeID = -1; 

	virtual void Activate();
	virtual void Deactivate();
	virtual void SetEnabled(bool enabled);
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
	virtual void PostLoad();

	void DrawLayerMaskUI(const char* label, uint16_t* layer);
	void calculateBoundingCircle();
	void calculateBoundingCircle(CollisionShapeEntry& entry);

	int AddShape(Shape shape, std::string name = "");
	void RemoveShape(int shapeId);
	CollisionShapeEntry* GetShape(int shapeId);
	CollisionShapeEntry* GetResolutionShape();
	int GetShapeId(const std::string& name);
	void SetResolutionShapeID(int shapeId);

	void SetShape(CollisionShapeEntry& entry, Shape shape);
	std::vector<glm::vec3> VerticesFromShape(Shape& shape);
	glm::vec3 GetCenter(CollisionShapeEntry& entry);
	float GetArea(CollisionShapeEntry& entry);

	void SetSyncWithRenderComponent(CollisionShapeEntry& entry, bool sync);
	void SyncFromRenderComponent(CollisionShapeEntry& entry);

	void SetShape(Shape shape);
	glm::vec3 GetCenter();
	float GetArea();

	void Draw();

	void SetCollisionLayer(uint16_t layer);
	void SetCollisionMask(uint16_t mask);

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
	int nextShapeID = 0;

	void RebuildFromShape(CollisionShapeEntry& entry, const std::vector<glm::vec3>& localVerts);
	void ApplyLiveShapeUpdate(CollisionShapeEntry& entry, const std::vector<glm::vec3>& verts);
	void SyncResolutionShapeFields();

	void ProcessShapeEntryUI(CollisionShapeEntry& entry);
	void DrawShapePreview(CollisionShapeEntry& entry);
};