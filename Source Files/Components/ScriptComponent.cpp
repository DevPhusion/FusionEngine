#include "../../Header Files/Components/ScriptComponent.h"

namespace py = pybind11;

ScriptComponent::ScriptComponent(Object* parent, std::string sourcePath) : ComponentBase<ScriptComponent>(parent) {
	SetSourcePath(sourcePath);
	this->Name = "Script Component";
}

std::string ScriptComponent::GetDisplayName() {
	if (sourcePath.empty()) return "";
	return std::filesystem::path(sourcePath).stem().string();
}

void ScriptComponent::SetSourcePath(std::string path) {
	sourcePath = path;
	Unload(); // force a fresh load (and clear any previous error) next run
}

void ScriptComponent::OnDelete() {
	Unload();
}

void ScriptComponent::ProcessInspectorUI() {

}

void ScriptComponent::CopyTo(Object* other) {
	ScriptComponent* target = other->GetComponent<ScriptComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<ScriptComponent>(other, ""));
		target = other->GetComponent<ScriptComponent>();
	}

	target->SetSourcePath(sourcePath);
}

std::unique_ptr<Component> ScriptComponent::Clone(Object* parent) {
	std::unique_ptr<ScriptComponent> comp = std::make_unique<ScriptComponent>(parent, "");
	comp->SetSourcePath(sourcePath);
	return comp;
}

void ScriptComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(sourcePath);
}

void ScriptComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	SetSourcePath(r.ReadString());
}

void ScriptComponent::Unload() {
	if (scriptInstance || scriptModule) {
		py::gil_scoped_acquire gil;
		scriptInstance = py::object();
		scriptModule = py::object();
	}
	loaded = false;
	loadFailed = false;
}

void ScriptComponent::EnsureLoaded() {
	if (loaded || loadFailed) return;
	loadFailed = true; 

	if (sourcePath.empty()) return;
	if (!Py_IsInitialized()) {
		Console::PrintError("ScriptComponent: Python backend isn't ready yet; cannot load {}").Format(sourcePath);
		return;
	}

	std::filesystem::path absPath = FileManager::getInstance().VirtualToAbsolute(sourcePath);
	std::error_code ec;
	if (!std::filesystem::exists(absPath, ec)) {
		Console::PrintError("ScriptComponent: script file not found: {}").Format(absPath.string());
		return;
	}

	std::ifstream in(absPath);
	if (!in.is_open()) {
		Console::PrintError("ScriptComponent: failed to open script file: {}").Format(absPath.string());
		return;
	}
	std::ostringstream sourceBuf;
	sourceBuf << in.rdbuf();
	std::string source = sourceBuf.str();

	py::gil_scoped_acquire gil;

	try {
		std::string moduleName = "fusion_script_" + absPath.stem().string();

		py::object typesModule = py::module_::import("types");
		py::object moduleObj = typesModule.attr("ModuleType")(moduleName);
		py::dict moduleDict = moduleObj.attr("__dict__");
		moduleDict["__name__"] = moduleName;
		moduleDict["__file__"] = absPath.string();

		py::exec(source, moduleDict, moduleDict);

		py::object fusionModule = py::module_::import("fusion");
		py::object scriptBaseClass = fusionModule.attr("Script");

		py::object foundClass;
		for (auto item : moduleDict) {
			py::object value = py::reinterpret_borrow<py::object>(item.second);
			if (!PyType_Check(value.ptr())) continue;
			if (value.is(scriptBaseClass)) continue;

			int isSub = PyObject_IsSubclass(value.ptr(), scriptBaseClass.ptr());
			if (isSub == 1) {
				foundClass = value;
				break; 
			}
			else if (isSub < 0) {
				PyErr_Clear();
			}
		}

		if (!foundClass) {
			Console::PrintError("ScriptComponent: no class inheriting from Script found in {}").Format(absPath.string());
			return;
		}

		scriptModule = moduleObj;
		scriptInstance = foundClass();

		loaded = true;
		loadFailed = false;
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: failed to load script {}: {}").Format(absPath.string(), e.what());
		scriptModule = py::object();
		scriptInstance = py::object();
	}
}

void ScriptComponent::RunOnStart() {
	EnsureLoaded();
	if (!loaded) return;

	py::gil_scoped_acquire gil;
	try {
		if (py::hasattr(scriptInstance, "OnStart"))
			scriptInstance.attr("OnStart")();
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: error in OnStart() for {}: {}").Format(sourcePath, e.what());
		loaded = false;
		loadFailed = true;
	}
}

void ScriptComponent::RunProcess(float delta) {
	if (!loaded) return; 

	py::gil_scoped_acquire gil;
	try {
		if (py::hasattr(scriptInstance, "Process"))
			scriptInstance.attr("Process")(delta);
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: error in Process() for {}: {}").Format(sourcePath, e.what());
		loaded = false;
		loadFailed = true;
	}
}