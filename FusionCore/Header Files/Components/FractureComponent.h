#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include "../Core/EngineManager.h"
#include "RigidBodyComponent.h"
class FractureComponent : public ComponentBase<FractureComponent>
{
public:
	FractureComponent(Object* parent);
	FractureComponent() = default;

	bool fracturable = true;
    float impulseThreshold = 5.0f;   
    int   shardCount = 5;      
    float minFragmentArea = 0.02f; 
    int   maxFractureGenerations = 2;
	float restDensity = 1.0f; // used for static object
    int   generation = 1;

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
};

