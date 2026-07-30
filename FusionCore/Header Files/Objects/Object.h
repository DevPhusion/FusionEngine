#pragma once
#include "../Components/Component.h"
#include "../Components/ComponentTypeID.h"
#include "../Core/Rendering/Shader.h"
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <memory>

template <typename T>
concept AllowedTypes = std::is_base_of_v<Component, T>;

class Object
{
public:
	Object(Shader shader);
	Object() = default;

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;
	Object(Object&&) = default;
	Object& operator=(Object&&) = default;

	uint64_t id = NextID();

	std::string name;
	bool hideInHierarchy = false;
	bool hidden = false;
	std::vector<std::unique_ptr<Component>> components = {};
	std::vector<Component*> componentByType;
	Object* parent = nullptr;
	uint64_t parentID = -1;
	std::vector<Object*> children;
	Shader shader;

	void SetParent(Object* newParent) {
		parent = newParent;
		parentID = newParent ? static_cast<int>(newParent->id) : -1;
		if (parent) {
			parent->children.push_back(this);
		}
	}

	void Show();
	void Hide();

	template <AllowedTypes T>
	T* GetComponent() {
		size_t id = ComponentTypeID::Get<T>();
		if (id >= componentByType.size()) return nullptr;
		return static_cast<T*>(componentByType[id]);
	}

	template <AllowedTypes T>
	bool HasComponent() {
		return GetComponent<T>() != nullptr;
	}

	void RemoveComponent(int index) {
		size_t id = components[index]->GetTypeID();
		components[index]->OnDelete();
		if (id < componentByType.size() && componentByType[id] == components[index].get()) {
			componentByType[id] = nullptr;
		}
		components.erase(components.begin() + index);
	}

	template <AllowedTypes T>
	void RemoveComponent() {
		size_t id = ComponentTypeID::Get<T>();
		if (id >= componentByType.size()) return;

		Component* target = componentByType[id];
		if (!target) return;

		for (int i = 0; i < components.size(); i++)
		{
			if (components[i].get() == target) {
				components[i]->OnDelete();
				componentByType[id] = nullptr;
				components.erase(components.begin() + i);
				return;
			}
		}
	}

	virtual void OnDelete() {
		auto safeCallbacks = OnDeleteCallbacks;
		for (const auto& [id, func] : safeCallbacks) {	
			if (func) {
				func();
			}
		}

		for (int i = 0; i < components.size(); i++)
		{
			components[i]->OnDelete();
		}
	}

	std::unique_ptr<Object> Clone() {
		std::unique_ptr<Object> obj = std::make_unique<Object>(shader);
		for (int i = 0; i < this->components.size(); i++)
		{
			obj->AddComponent(this->components[i]->Clone(obj.get()));
		}
		obj->id = id;
		obj->name = name;
		obj->parentID = parentID;
		obj->hidden = hidden;
		return obj;
	}

	void Serialize(BinaryWriter& w);

	void Deserialize(BinaryReader& r);

	void AddComponent(std::unique_ptr<Component> component) {
		size_t id = component->GetTypeID(); 
		if (id >= componentByType.size()) componentByType.resize(id + 1, nullptr);
		componentByType[id] = component.get();
		components.push_back(std::move(component));
	}

	void RegisterComponentPointer(Component* comp) {
		size_t id = comp->GetTypeID();
		if (id >= componentByType.size()) componentByType.resize(id + 1, nullptr);
		componentByType[id] = comp;
	}

	std::unordered_map<int, std::function<void()>> OnDeleteCallbacks;

	int AddOnDeleteCallback(std::function<void()> func);
	void RemoveOnDeleteCallback(int ID);
private:
	int CurrentOnRemoveID = -1;

	static uint64_t NextID() {
		static uint64_t counter = 1;
		return counter++;
	}
};

