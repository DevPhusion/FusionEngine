#pragma once
#include "Component.h"
#include "PointMass.h"
#include "XPBDDistanceConstraint.h"
#include "XPBDAreaConstraint.h"
#include "Renderer.h"
#include <variant>

struct SoftEdge {
	Edge edge;
	int idxA;
	int idxB;
	glm::vec3 normal;
};

struct VirtualLink {
	PointMass* virtualPM;
	PointMass* realPM;
	float weight;
};

class SoftBodyComponent : public ComponentBase<SoftBodyComponent>
{
public:
	SoftBodyComponent(Object* parent);
	SoftBodyComponent() = default;

	std::vector<std::unique_ptr<PointMass>> VirtualMassAggregate = {};
	std::vector<std::unique_ptr<PointMass>> MassAggregate = {};
	std::vector<VirtualLink> virtualLinks;
	std::vector<XPBDDistanceConstraint*> springs = {};
	XPBDAreaConstraint* areaConstraint = nullptr;

	PointMass* CenterPM = nullptr;

	bool isDragging;

	float virtualPointPercentClosest = 0.2f;
	float inverseMass = 1.0f;
	float stiffness = 150.0f;
	float damping = 2.47f;

	void ProcessSoftBody(float delta);
	void BuildMassAggregate();
	void RebuildMassAggregate();
	void UpdateMassAggregate();
	void SyncMeshFromMassAggregate();
	void DrawSprings();
	std::vector<SoftEdge> GetEdgesFromMassAggregate();

	PointMass* AddVirtualPointMass(glm::vec3 point);
	void RemoveVirtualPointMass(PointMass* pm);
	void ProcessVirtualPointMass(float delta);

	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual void CopyTo(Object* other);
private:
	bool updatingFromPoints = false;
	bool updatingFromParent = false;
	int setShapeCallbackID;
	int transformCallbackID;
};

