#include "../../Header Files/Components/Component.h"
#include "../../Header Files/Objects/Object.h"

Component::Component(Object* parent) {
	this->parent = parent;
}

void Component::SetEnabled(bool enabled) {
	Enabled = enabled;
}