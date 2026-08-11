#include "../../Header Files/Objects/Object.h"
#include "../../Header Files/Components/Components.h"

Object::Object(Shader shader) {
	this->shader = shader;
}

void Object::Show() {
	hidden = false;
	for (auto* obj : children) {
		obj->Show();
	}
}

void Object::Hide() {
	hidden = true;
	for (auto* obj : children) {
		obj->Hide();
	}
}

int Object::AddOnDeleteCallback(std::function<void()> func) {
	CurrentOnRemoveID += 1;
	OnDeleteCallbacks[CurrentOnRemoveID] = func;
	return CurrentOnRemoveID;
}

void Object::RemoveOnDeleteCallback(int ID) {
	OnDeleteCallbacks.erase(ID);
}

void Object::Serialize(BinaryWriter& w) {
	w.Write(id);
	w.WriteString(name);
	w.Write(hideInHierarchy);
	w.Write(hidden);
	w.Write(parentID);

	w.WriteString(shader.vertexPath);
	w.WriteString(shader.fragmentPath);

	w.Write(static_cast<uint32_t>(components.size()));
	for (auto& c : components)
	{
		c->Serialize(w);
	}
}

void Object::Deserialize(BinaryReader& r) {
    this->id = r.Read<uint64_t>();
    Object::ReserveID(id);
    DeserializeBody(r);
}

void Object::DeserializeBody(BinaryReader& r) {
    this->name = r.ReadString();
    this->hideInHierarchy = r.Read<bool>();
    this->hidden = r.Read<bool>();
    this->parentID = r.Read<uint64_t>();
    std::string vertPath = r.ReadString();
    std::string fragPath = r.ReadString();
    this->shader = Shader(vertPath.c_str(), fragPath.c_str());

    uint32_t count = r.Read<uint32_t>();
    for (uint32_t i = 0; i < count; i++) {
        std::string typeName = r.ReadString();
        auto comp = CreateComponentFromName(this, typeName);
        if (!comp) throw std::runtime_error("Object::Deserialize: unknown component type '" + typeName + "'");
        RegisterComponentPointer(comp.get());
        comp->Deserialize(r);
        AddComponent(std::move(comp));
    }
}

void Object::ApplyState(BinaryReader& r) {
	this->name = r.ReadString();
	this->hideInHierarchy = r.Read<bool>();
	this->hidden = r.Read<bool>();
	this->parentID = r.Read<uint64_t>();
	r.ReadString();
	r.ReadString();

	uint32_t count = r.Read<uint32_t>();

	std::vector<std::unique_ptr<Component>> oldComponents = std::move(components);
	components.clear();
	componentByType.clear();

	std::vector<std::unique_ptr<Component>> newComponents;
	newComponents.reserve(count);

	for (uint32_t i = 0; i < count; i++) {
		std::string typeName = r.ReadString();

		std::unique_ptr<Component> comp;

		if (i < oldComponents.size() && oldComponents[i] && oldComponents[i]->Name == typeName) {
			comp = std::move(oldComponents[i]);
		}
		else {
			auto it = std::find_if(oldComponents.begin(), oldComponents.end(),
				[&](const std::unique_ptr<Component>& c) { return c && c->Name == typeName; });

			if (it != oldComponents.end()) {
				comp = std::move(*it);
			}
			else {
				comp = CreateComponentFromName(this, typeName);
				if (!comp) throw std::runtime_error("Object::ApplyState: unknown component type '" + typeName + "'");
			}
		}

		RegisterComponentPointer(comp.get());
		comp->Deserialize(r);
		newComponents.push_back(std::move(comp));
	}

	for (auto& old : oldComponents) {
		if (old) old->OnDelete();
	}

	for (auto& c : newComponents) {
		AddComponent(std::move(c));
	}
}