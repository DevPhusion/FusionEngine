#pragma once
#include "Component.h"
#include "../Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "../Core/Physics/PhysicsEngine.h"
#include "SoftBodyComponent.h"

class ConstraintComponent : public ComponentBase<ConstraintComponent> {
public:
	ConstraintComponent(Object* parent);
	ConstraintComponent() = default;

	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	std::vector<Constraint*> mirroredConstraints; // Constraint that other objects applied to this
	std::vector<std::shared_ptr<Constraint>> appliedConstraints;

	void AddConstraint(std::shared_ptr<Constraint> constraint);

	void RemoveConstraint(Constraint* constraint);

	void RemoveConstraint(std::size_t index);
};