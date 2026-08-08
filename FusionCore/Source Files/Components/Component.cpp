#include "../../Header Files/Components/Component.h"
#include "../../Header Files/Objects/Object.h"
#include "../../Header Files/Core/EngineManager.h"

Component::Component(Object* parent) {
	this->parent = parent;
}

void Component::SetEnabled(bool enabled) {
	Enabled = enabled;
	EngineManager::getInstance().EngineChangeEvent();
}