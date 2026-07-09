#pragma once
#include "Component.h"
#include "PointMass.h"
#include "XPBDDistanceConstraint.h"
#include "XPBDAreaConstraint.h"
#include "XPBDProxyPointConstraint.h"
#include "Renderer.h"
#include <variant>

struct SoftEdge {
	Edge edge;
	int idxA;
	int idxB;
	glm::vec3 normal;
};

struct VirtualLink {
	PointMass* virtualProxy;
	std::vector<PointMass*> affectPM;
	std::vector<glm::vec3> localOffsets;
};

class SoftBodyComponent : public ComponentBase<SoftBodyComponent>
{
public:
	SoftBodyComponent(Object* parent);
	SoftBodyComponent() = default;

	std::vector<std::unique_ptr<PointMass>> VirtualMassAggregate = {};
	std::vector<std::unique_ptr<PointMass>> MassAggregate = {};
	std::vector<VirtualLink> virtualLinks = {};
	std::vector<XPBDProxyPointConstraint*> proxyLinks = {};
	std::vector<XPBDDistanceConstraint*> springs = {};
	XPBDAreaConstraint* areaConstraint = nullptr;

	PointMass* CenterPM = nullptr;

	bool isDragging;

	float attachmentStiffness = 3000.0f;
	float virtualPointPercentClosest = 0.001f;

	float inverseMass = 1.0f;
	float stiffness = 150.0f;
	float damping = 2.47f;

	void ProcessSoftBody(float delta);
	void BuildMassAggregate();
	void RebuildMassAggregate();
	void UpdateMassAggregate();
	void SyncMeshFromMassAggregate();
	void DrawSprings();
	void PreProxySync(float delta);
	float CalculateVirtualRigidBodyInvInertia(glm::vec3 pos);
	PointMass* AddVirtualRigidBody(glm::vec3 localPos);
	PhysicsBody FindClosestPointMassBody(glm::vec3 localPoint, float* outWeight);
	std::vector<SoftEdge> GetEdgesFromMassAggregate();

	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual void CopyTo(Object* other);
private:
	bool updatingFromPoints = false;
	bool updatingFromParent = false;
	int setShapeCallbackID;
	int transformCallbackID;
};

