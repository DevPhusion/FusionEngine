#pragma once
#include "../Objects/Polygon.h"
#include "../Objects/Box.h"
#include "../Objects/Circle.h"
#include "../Objects/VertexPoint.h"
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
	std::vector<VertexPoint*> vertexPoints;
	std::vector<float> vertices;

	void AddObject();
	void AddPolygon();
	void AddBox();
	void AddCircle();
	void AddSoftBox();
	void AddSoftCircle();
	void AddSoftPolygon();
	void AddPolygonVertex();
	void AddFluid();
	VertexPoint* CopyVertex(VertexPoint* vert); // For copying polygon
	Object* CopyObject(Object* obj);
	void RemoveObject(Object* obj);

	void ProcessObjects(float delta);

private:
	ObjectManager() = default;
};

