#pragma once
#include "Object.h"
#include "../Core/Physics/Constraint/PGSConstraint/Constraint.h"

class SoftBodyComponent;

class PointMass
{
public:
	PointMass(Shader shader, SoftBodyComponent* sb, glm::vec3 point, int index, bool isCenter);
	PointMass() = default;

	SoftBodyComponent* sb;
	Shader shader;

	int index;
	bool isCenter;
	float pointRadius = 0.01f;
	glm::vec3 localPos = glm::vec3(0);
	glm::vec3 worldPos = glm::vec3(0);
	glm::vec3 prevPos = glm::vec3(0);
	glm::mat4 transform = glm::mat4(1);
	glm::vec3 rotationCenter = glm::vec3(0);

	PhysicsBody body;
	float inverseMass = 1.0f; 
	float linearDamping = 0.995f;
	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 baseAcceleration = glm::vec3(0, -9.8f, 0);
	glm::vec3 acceleration = glm::vec3(0);

	//Simply to satisfy the PhysicsBody interface, pm can't rotate
	float rotation = 0; 
	float prevRotation = 0;
	float InverseInertia = 0; 
	float angularVelocity = 0; 

	void OnDelete();

	PhysicsBody BuildPhysicsBody();
	void ProcessTransform();
	void UpdateWorldPosition(glm::vec3 newPos);
	glm::vec3 ProjectToWorld(glm::vec3 point);
	glm::vec3 GetWorldPosition();
	
	void ProcessDragForce();

	void DrawDebug();

private:
	void InitDebugQuad();
	GLuint debugVAO = 0, debugVBO = 0, debugEBO = 0;
	std::vector<float> debugVertices;
	std::vector<unsigned int> debugIndices;
	glm::vec4 debugColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	float debugSize = 0.015f; 
	bool debugInitialized = false;
};

