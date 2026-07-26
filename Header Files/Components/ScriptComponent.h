#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include "../Core/Files/FileManager.h"
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <fstream>
#include <sstream>
class ScriptComponent : public ComponentBase<ScriptComponent>
{
public:
	ScriptComponent(Object* parent, std::string sourcePath);
	ScriptComponent() = default;

	std::string sourcePath = "";
	
	std::string GetDisplayName();
	void SetSourcePath(std::string path);

	void RunOnStart();
	void RunProcess(float delta);
	void Unload();

	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual std::unique_ptr<Component> Clone(Object* parent);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);
private:
	void EnsureLoaded();

	pybind11::object scriptModule;
	pybind11::object scriptInstance;
	bool loaded = false;
	bool loadFailed = false;
};

