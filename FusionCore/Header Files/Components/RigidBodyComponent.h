#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include "../Objects/Object.h"
#include <glm/glm.hpp>

class RigidBodyComponent : public ComponentBase<RigidBodyComponent>
{
public:
	RigidBodyComponent(Object* parent);
	RigidBodyComponent() = default;

	//Dragging
	bool isDragging = false;

	//For UI display
	glm::vec2 netForceDisplay = glm::vec3(0);
	glm::vec2 netAcceleration = glm::vec3(0);
	float torqueDisplay = 0;
	float angularAcceleration = 0;

	//Physics process
	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 acceleration = glm::vec3(0);
	glm::vec3 netForce = glm::vec3(0);

	float angularDamping = 0.995f;
	float linearDamping = 0.995f;
	float angularVelocity;
	float Torque;
	float Inertia;
	float inverseInertia;

	float inverseMass; 

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void IntegrateVelocities(float delta);
	void IntegratePositions(float delta);
	void ClearAccumulators();
	void ProcessDragForce();
	void AddForce(glm::vec3 force);
	void AddForceAtBodyPoint(glm::vec3 force, glm::vec3 point); // point is in model space
	void AddForceAtPoint(glm::vec3 force, glm::vec3 point); // point is in world space
	void CalculateInertia();
};

