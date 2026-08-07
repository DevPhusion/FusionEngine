#pragma once
#include "Rendering/Shader.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/RigidBodyComponent.h"
#include "../Components/MouseInteractComponent.h"
#include "../Components/CollisionComponent.h"
#include "../Components/ConstraintComponent.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <string>
#include "InputManager.h"
class ObjectManager
{
public:
	ObjectManager(const ObjectManager&) = delete;
	void operator=(const ObjectManager&) = delete;

	static ObjectManager& getInstance() {
		static ObjectManager instance;
		return instance;
	}

	std::vector<std::unique_ptr<Object>> allObjects;
	std::vector<std::unique_ptr<Object>> pendingObjects;
	std::vector<Object*> pendingRemovals;

	void AddObject(Object* parent = nullptr);
	void AddCamera(Object* parent = nullptr);
	void AddPolygon(Object* parent = nullptr);
	void AddBox(Object* parent = nullptr);
	void AddCircle(Object* parent = nullptr);
	void AddSoftBox(Object* parent = nullptr);
	void AddSoftCircle(Object* parent = nullptr);
	void AddSoftPolygon(Object* parent = nullptr);
	void AddFluid(Object* parent = nullptr);
	Object* AddExistingObject(std::unique_ptr<Object> obj, Object* parent = nullptr);
	void FlushPendingObjects();
	std::string GenerateUniqueName(const std::string& baseName, Object* exclude = nullptr);
	Object* FindObjectById(uint64_t id);
	Object* CopyObject(Object* obj);
	void RemoveObject(Object* obj);
	void QueueRemoveObject(Object* obj);
	void FlushPendingRemovals();

private:
	ObjectManager() = default;
	std::vector<float> BuildInterleavedVertices(const std::vector<glm::vec3>& localVerts);
};

