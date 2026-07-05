#pragma once

struct ContactID {
	int referenceEdgeA;
	int incidentEdgeB;
	int vertexTypeA; // 0: type face boundary, 1: type vertex
	int vertexTypeB;
};

struct ContactCache {
	Object* objectA = nullptr;
	Object* objectB = nullptr;
	void* pmA = nullptr; 
	void* pmB = nullptr;
	ContactID id;
	float lambda;
};