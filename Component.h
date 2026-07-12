#pragma once
#include <string>
#include <memory>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ComponentTypeID.h"

class Object;

class Component
{
public:
	Component(Object* parent);
	Component() = default;
 
	Object* parent;
	bool Enabled = true;

	bool CanDisable = true;
	bool CanRemove = true;
	bool Hidden = false;

	std::string Name;
	virtual void SetEnabled(bool enabled);
	virtual size_t GetTypeID() const = 0;
	virtual void CopyTo(Object* other) = 0;
	virtual std::unique_ptr<Component> Clone(Object* parent) = 0;
	virtual void ProcessInspectorUI() = 0;
	virtual void OnDelete() = 0;
};

template <typename Derived>
class ComponentBase : public Component
{
public:
	using Component::Component; 

	size_t GetTypeID() const override {
		return ComponentTypeID::Get<Derived>();
	}
};
