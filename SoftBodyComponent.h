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

class SoftBodyComponent : public ComponentBase<SoftBodyComponent>
{
public:
	SoftBodyComponent(Object* parent);
	SoftBodyComponent() = default;

	std::vector<std::unique_ptr<PointMass>> VirtualProxies = {};
	std::vector<std::unique_ptr<PointMass>> MassAggregate = {};
	std::vector<XPBDProxyPointConstraint*> proxyLinks = {};
	std::vector<XPBDDistanceConstraint*> springs = {};
	XPBDAreaConstraint* areaConstraint = nullptr;

	PointMass* CenterPM = nullptr;

	bool isDragging;
	bool useGasPressure = false;

	float attachmentStiffness = 3000.0f;
	float virtualPointPercentClosest = 0.01f;

	float inverseMass = 1.0f;
	float stiffness = 150.0f;
	float damping = 2.47f;
	float gasAmount = 1.0f;

	glm::vec3 prevScale = glm::vec3(1);

	void ProcessSoftBody(float delta);
	void ApplyGasPressure();
	void BuildMassAggregate();
	void RebuildMassAggregate();
	void UpdateMassAggregate();
	void SyncMeshFromMassAggregate();
	void DrawSprings();

	float CalculateVirtualProxyInvInertia(glm::vec3 pos);
	PointMass* AddVirtualProxy(glm::vec3 localPos);
	void UpdateVirtualProxy(PointMass* proxy);
	void RemoveVirtualProxy(PointMass* proxy);
	std::vector<SoftEdge> GetEdgesFromMassAggregate();

	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual void CopyTo(Object* other);
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
private:
	bool updatingFromPoints = false;
	bool updatingFromParent = false;
	int setShapeCallbackID;
	int transformCallbackID;
};

