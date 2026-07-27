#pragma once
#include "ForceGenerator.h"
class MouseDrag : public ForceGenerator
{
public:
	MouseDrag(float k, float c);

	// Mouse drag coefficient (damped spring)
	float k;
	float c;

	virtual void updateForce(Object* object, float delta);
	virtual void processDisplay(int index);
};

