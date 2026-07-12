#pragma once
#include "Component.h"
#include "Constraint.h"
#include "PhysicsEngine.h"
#include "SoftBodyComponent.h"

class ConstraintComponent : public ComponentBase<ConstraintComponent> {
public:
	ConstraintComponent(Object* parent);
	ConstraintComponent() = default;

	virtual void ProcessInspectorUI();
	virtual void OnDelete();
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void CopyTo(Object* other);

	std::vector<Constraint*> mirroredConstraints; // Constraint that other objects applied to this
	std::vector<std::shared_ptr<Constraint>> appliedConstraints;

	void AddConstraint(std::shared_ptr<Constraint> constraint);

	void RemoveConstraint(Constraint* constraint);

	void RemoveConstraint(std::size_t index);
};