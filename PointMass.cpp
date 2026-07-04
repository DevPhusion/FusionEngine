#include "PointMass.h"
#include "SoftBodyComponent.h"

PointMass::PointMass(Shader shader, SoftBodyComponent* sb, glm::vec3 point, int index, bool isCenter) {
	this->sb = sb;
	this->index = index;
	this->isCenter = isCenter;
	this->shader = shader;
	this->transform = Camera::getInstance().viewMatrixInverse;

	InitDebugQuad();
	UpdateWorldPosition(point);
	body = BuildPhysicsBody();
}

PhysicsBody PointMass::BuildPhysicsBody() {
	PhysicsBody body = PhysicsBody();
	body.obj = nullptr;
	body.position = &worldPos;
	body.prevPos = &prevPos;
	body.transformMatrix = &transform;
	body.rotation = &rotation;
	body.velocity = &velocity;
	body.angularVelocity = &angularVelocity;
	body.invMass = &inverseMass;
	body.invInertia = &InverseInertia;
	return body;
}

void PointMass::InitDebugQuad() {
	float h = debugSize;

	debugVertices = {
		-h, -h, 0.0f,   0.0f, 0.0f,
		 h, -h, 0.0f,   1.0f, 0.0f,
		 h,  h, 0.0f,   1.0f, 1.0f,
		-h,  h, 0.0f,   0.0f, 1.0f,
	};

	debugIndices = { 0, 1, 2,  2, 3, 0 };

	glGenVertexArrays(1, &debugVAO);
	glBindVertexArray(debugVAO);

	glGenBuffers(1, &debugVBO);
	glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
	glBufferData(GL_ARRAY_BUFFER, debugVertices.size() * sizeof(float), debugVertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &debugEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debugEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, debugIndices.size() * sizeof(unsigned int), debugIndices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	debugInitialized = true;
}

void PointMass::ProcessDragForce() {
	glm::vec3 currentWorldPos = GetWorldPosition();

	TransformComponent* trans = sb->parent->GetComponent<TransformComponent>();
	glm::vec3 modelPos = trans->GetTransformedPoint(glm::vec3(InputManager::glX, InputManager::glY, 0), true);
	glm::vec3 MouseWorldPos = trans->ProjectToWorld(modelPos);
	
	glm::vec3 delta = MouseWorldPos - currentWorldPos;

	glm::vec3 springForce = 150.0f * (1 / inverseMass) * delta;
	glm::vec3 dampingForce = -24.5f * (1 / inverseMass) * velocity;
	glm::vec3 totalForce = springForce + dampingForce;
	acceleration = totalForce * inverseMass;
}

void PointMass::UpdateWorldPosition(glm::vec3 newPos) {
	glm::vec4 center = glm::vec4(rotationCenter.x, rotationCenter.y, rotationCenter.z, 1.0f);
	glm::vec4 currentWorldPos = transform * center;

	glm::vec3 currentPosVec3 = glm::vec3(currentWorldPos.x, currentWorldPos.y, currentWorldPos.z);
	glm::vec3 delta = newPos - currentPosVec3;

	glm::mat4 newTransform = glm::translate(glm::mat4(1.0f), delta) * transform;

	transform = newTransform;
	prevPos = worldPos;
	worldPos = GetWorldPosition();
}

glm::vec3 PointMass::GetWorldPosition() {
	return ProjectToWorld(rotationCenter);
}

glm::vec3 PointMass::ProjectToWorld(glm::vec3 point) {
	glm::vec4 p = glm::vec4(point.x, point.y, point.z, 1.0f);
	return glm::vec3(transform * p);
}

void PointMass::ProcessTransform() {
	this->shader.use();
	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
	this->shader.setMat4D("projection", projection);
	this->shader.setMat4D("transform", this->transform);
	this->shader.setMat4D("view", Camera::getInstance().viewMatrix);

	if (worldPos != GetWorldPosition()) {
		prevPos = GetWorldPosition();
		UpdateWorldPosition(worldPos);
	}
}

void PointMass::DrawDebug() {
	if (!debugInitialized) return;

	glm::vec3 worldPosition = GetWorldPosition();

	glm::vec4 screenPos = Camera::getInstance().viewMatrix * glm::vec4(worldPosition, 1.0f);
	glm::mat4 markerTransform = glm::translate(glm::mat4(1.0f), glm::vec3(screenPos));

	glm::mat4 projection = glm::ortho(-EngineManager::getInstance().aspectRatio, EngineManager::getInstance().aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);

	this->shader.use();
	this->shader.setVec4D("aColor", debugColor);
	this->shader.setMat4D("projection", projection);
	this->shader.setMat4D("transform", markerTransform);
	this->shader.setMat4D("view", glm::mat4(1.0f)); 

	glBindVertexArray(debugVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debugEBO);
	glDrawElements(GL_TRIANGLES, (GLsizei)debugIndices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void PointMass::OnDelete() {
	if (!debugInitialized) return;
	glDeleteBuffers(1, &debugVBO);
	glDeleteBuffers(1, &debugEBO);
	glDeleteVertexArrays(1, &debugVAO);
	debugInitialized = false;
}