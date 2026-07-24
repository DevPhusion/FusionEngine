#pragma once
#include "../../../Objects/Object.h"
#include "../../../Components/RigidBodyComponent.h"
class ForceGenerator
{
public:
	virtual void updateForce(Object* object, float delta) = 0;
	virtual void processDisplay(int index) = 0;
	void setDisplayFunc(std::shared_ptr<std::function<void(int index)>> func); // store a pointer to the display func (to identify the function for removal)

	std::shared_ptr<std::function<void(int index)>> displayFunc;
	glm::vec3 Force;
};

