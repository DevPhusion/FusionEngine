#pragma once
#include "../Core/Files/BinaryReader.h"
#include "../Core/Files/BinaryWriter.h"
#include <string>
#include <memory>
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_glfw.h"
#include "../../imgui/imgui_impl_opengl3.h"
#include "ComponentTypeID.h"

class Object;

class Component
{
public:
	Component(Object* parent);
	Component() = default;
 
	Object* parent;
	bool isActive = false;
	bool Enabled = true;
	bool pendingEnabled = true;

	bool CanDisable = true;
	bool CanRemove = true;
	bool Hidden = false;

	std::string Name;

	virtual void Activate() {}
	virtual void Deactivate() {}
	virtual void SetEnabled(bool enabled);
	virtual size_t GetTypeID() const = 0;
	virtual void CopyTo(Object* other) = 0;
	virtual void PostLoad() {};
	virtual void ProcessInspectorUI() = 0;
	virtual void OnDelete() = 0;

	virtual void Serialize(BinaryWriter& w)
	{
		w.WriteString(Name);
		w.Write(Enabled);
		w.Write(CanDisable);
		w.Write(CanRemove);
		w.Write(Hidden);
	}

	virtual void Deserialize(BinaryReader& r)
	{
		Enabled = r.Read<bool>();
		CanDisable = r.Read<bool>();
		CanRemove = r.Read<bool>();
		Hidden = r.Read<bool>();
	}
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
