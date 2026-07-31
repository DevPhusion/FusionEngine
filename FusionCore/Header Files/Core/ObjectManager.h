#pragma once
#include "../Objects/VertexPoint.h"
#include "Rendering/Shader.h"
#include "../Components/RenderComponent.h"
#include "../Components/TransformComponent.h"
#include "../Components/VertexComponent.h"
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
	std::vector<VertexPoint*> vertexPoints;
	std::vector<float> vertices;

	void AddObject();
	void AddCamera();
	void AddPolygon();
	void AddBox();
	void AddCircle();
	void AddSoftBox();
	void AddSoftCircle();
	void AddSoftPolygon();
	void AddPolygonVertex();
	void AddFluid();
	Object* AddExistingObject(std::unique_ptr<Object> obj, Object* parent = nullptr);
	void FlushPendingObjects();
	std::string GenerateUniqueName(const std::string& baseName, Object* exclude = nullptr);
	VertexPoint* CopyVertex(VertexPoint* vert); // For copying polygon
	Object* CopyObject(Object* obj);
	void RemoveObject(Object* obj);
	void QueueRemoveObject(Object* obj);
	void FlushPendingRemovals();

private:
	ObjectManager() = default;
};

