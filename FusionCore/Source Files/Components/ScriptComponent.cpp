#include "../../Header Files/Components/ScriptComponent.h"
#include "../../Header Files/Core/Scripting/PyBindings.h"
#include "../../Header Files/Core/EngineManager.h"

namespace py = pybind11;

namespace {
	void WriteExportedValue(BinaryWriter& w, const ExportedValue& value) {
		w.Write(static_cast<int>(value.index()));
		std::visit([&](auto&& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::string>) {
				w.WriteString(v);
			}
			else if constexpr (std::is_same_v<T, glm::vec2>) {
				w.Write(v.x);
				w.Write(v.y);
			}
			else {
				w.Write(v); 
			}
			}, value);
	}

	ExportedValue ReadExportedValue(BinaryReader& r) {
		int index = r.Read<int>();
		switch (index) {
		case 0: return r.ReadString();
		case 1: return r.Read<int>();
		case 2: return r.Read<float>();
		case 3: return r.Read<bool>();
		case 4: {
			float x = r.Read<float>();
			float y = r.Read<float>();
			return glm::vec2(x, y);
		}
		case 5: return r.Read<glm::vec3>();
		default: return std::string();
		}
	}
}

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
	Unload(); 
}

void ScriptComponent::OnDelete() {
	Unload();
}

void ScriptComponent::CopyTo(Object* other) {
	ScriptComponent* target = other->GetComponent<ScriptComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<ScriptComponent>(other, ""));
		target = other->GetComponent<ScriptComponent>();
	}

	target->SetSourcePath(sourcePath);
	target->pendingExportedValues = loaded ? exportedProperties : pendingExportedValues;
}

std::unique_ptr<Component> ScriptComponent::Clone(Object* parent) {
	std::unique_ptr<ScriptComponent> comp = std::make_unique<ScriptComponent>(parent, "");
	comp->SetSourcePath(sourcePath);
	comp->pendingExportedValues = loaded ? exportedProperties : pendingExportedValues;
	return comp;
}

void ScriptComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(sourcePath);

	const std::vector<ExportedProperty>& values = loaded ? exportedProperties : pendingExportedValues;
	w.Write(static_cast<int>(values.size()));
	for (auto& prop : values) {
		w.WriteString(prop.name);
		WriteExportedValue(w, prop.value);
	}
}

void ScriptComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	SetSourcePath(r.ReadString());

	int count = r.Read<int>();
	pendingExportedValues.clear();
	pendingExportedValues.reserve(count);
	for (int i = 0; i < count; i++) {
		ExportedProperty prop;
		prop.name = r.ReadString();
		prop.value = ReadExportedValue(r);
		pendingExportedValues.push_back(std::move(prop));
	}
}

void ScriptComponent::Unload() {
	if (scriptInstance || scriptModule) {
		py::gil_scoped_acquire gil;
		scriptInstance = py::object();
		scriptModule = py::object();
	}
	exportedProperties.clear();
	onLoadRan = false;
	loaded = false;
	loadFailed = false;
}

void ScriptComponent::CheckForFileChanges() {
	if (sourcePath.empty()) return;
	if (!loaded && !loadFailed) return; 

	auto now = std::chrono::steady_clock::now();
	if (now < nextFileCheckTime) return;
	nextFileCheckTime = now + kFileWatchInterval;

	std::filesystem::path absPath = FileManager::getInstance().VirtualToAbsolute(sourcePath);
	std::error_code ec;
	auto currentWriteTime = std::filesystem::last_write_time(absPath, ec);
	if (ec) return; 

	if (currentWriteTime != lastWriteTime) {
		lastWriteTime = currentWriteTime;
		Reload();
	}
}

void ScriptComponent::Reload() {
	std::vector<ExportedProperty> previousValues = std::move(exportedProperties);
	pendingExportedValues = std::move(exportedProperties);

	Unload();
	EnsureLoaded();

	if (loaded) {
		for (auto& prop : exportedProperties) {
			auto it = std::find_if(previousValues.begin(), previousValues.end(),
				[&](const ExportedProperty& p) { return p.name == prop.name; });

			if (it != previousValues.end() && it->value.index() == prop.value.index()) {
				prop.value = it->value;
				SetInstanceAttrFromVariant(prop.name, prop.value);
			}
		}
	}

	EngineManager::getInstance().EngineChangeEvent();
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

	std::error_code ecStat;
	lastWriteTime = std::filesystem::last_write_time(absPath, ecStat);

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

		try {
			scriptInstance.cast<ScriptBase&>().parent = this->parent;
		}
		catch (const py::cast_error&) {
			Console::PrintError("ScriptComponent: class in {} does not properly inherit from Script").Format(absPath.string());
			scriptModule = py::object();
			scriptInstance = py::object();
			return;
		}

		ScanExportedProperties();

		loaded = true;
		loadFailed = false;
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: failed to load script {}: {}").Format(absPath.string(), e.what());
		scriptModule = py::object();
		scriptInstance = py::object();
	}
}

void ScriptComponent::ScanExportedProperties() {
	exportedProperties.clear();
	if (!scriptInstance) return;

	py::gil_scoped_acquire gil;

	py::object cls = scriptInstance.attr("__class__");
	py::dict classDict = cls.attr("__dict__");

	for (auto item : classDict) {
		py::object attrValue = py::reinterpret_borrow<py::object>(item.second);
		if (!py::isinstance<ExportMarker>(attrValue)) continue;

		std::string attrName = py::str(item.first);
		py::object defaultValue = attrValue.cast<ExportMarker>().value;

		ExportedProperty prop;
		prop.name = attrName;

		if (py::isinstance<py::bool_>(defaultValue)) {
			prop.value = defaultValue.cast<bool>();
		}
		else if (py::isinstance<py::int_>(defaultValue)) {
			prop.value = defaultValue.cast<int>();
		}
		else if (py::isinstance<py::float_>(defaultValue)) {
			prop.value = defaultValue.cast<float>();
		}
		else if (py::isinstance<py::str>(defaultValue)) {
			prop.value = defaultValue.cast<std::string>();
		}
		else if (py::isinstance<glm::vec3>(defaultValue)) {
			prop.value = defaultValue.cast<glm::vec3>();
		}
		else if (py::isinstance<glm::vec2>(defaultValue)) {
			prop.value = defaultValue.cast<glm::vec2>();
		}
		else {
			Console::PrintWarning("ScriptComponent: exported property '{}' has an unsupported type; skipping").Format(attrName);
			continue;
		}

		SetInstanceAttrFromVariant(attrName, prop.value);
		exportedProperties.push_back(std::move(prop));
	}

	ApplyPendingExportedValues(); 
}

void ScriptComponent::ApplyPendingExportedValues() {
	if (pendingExportedValues.empty()) return;

	for (auto& prop : exportedProperties) {
		auto it = std::find_if(pendingExportedValues.begin(), pendingExportedValues.end(),
			[&](const ExportedProperty& p) { return p.name == prop.name; });

		if (it != pendingExportedValues.end() && it->value.index() == prop.value.index()) {
			prop.value = it->value;
			SetInstanceAttrFromVariant(prop.name, prop.value);
		}
	}

	pendingExportedValues.clear();
}

void ScriptComponent::RefreshExportedPropertiesFromInstance() {
	if (!scriptInstance) return;
	py::gil_scoped_acquire gil;

	for (auto& prop : exportedProperties) {
		if (!py::hasattr(scriptInstance, prop.name.c_str())) continue;
		py::object current = scriptInstance.attr(prop.name.c_str());

		std::visit([&](auto&& stored) {
			using T = std::decay_t<decltype(stored)>;
			try {
				stored = current.cast<T>();
			}
			catch (const py::cast_error&) {
				
			}
			}, prop.value);
	}
}

void ScriptComponent::SetInstanceAttrFromVariant(const std::string& name, const ExportedValue& value) {
	if (!scriptInstance) return;
	py::gil_scoped_acquire gil;

	std::visit([&](auto&& v) {
		scriptInstance.attr(name.c_str()) = py::cast(v);
		}, value);
}

void ScriptComponent::ProcessInspectorUI() {
	RunOnLoad();

	if (!loaded || exportedProperties.empty()) return;

	RefreshExportedPropertiesFromInstance();

	for (auto& prop : exportedProperties) {
		ImGui::PushID(prop.name.c_str());

		std::visit([&](auto&& value) {
			using T = std::decay_t<decltype(value)>;

			ImGui::Text("%s", prop.name.c_str());
			ImGui::SameLine();

			if constexpr (std::is_same_v<T, std::string>) {
				char buf[256];
				std::memset(buf, 0, sizeof(buf));
				std::size_t len = std::min(value.size(), sizeof(buf) - 1);
				std::memcpy(buf, value.data(), len);

				if (ImGui::InputText("##val", buf, sizeof(buf))) {
					value = std::string(buf);
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			else if constexpr (std::is_same_v<T, int>) {
				if (ImGui::InputInt("##val", &value)) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			else if constexpr (std::is_same_v<T, float>) {
				if (ImGui::InputFloat("##val", &value)) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			else if constexpr (std::is_same_v<T, bool>) {
				if (ImGui::Checkbox("##val", &value)) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			else if constexpr (std::is_same_v<T, glm::vec2>) {
				if (ImGui::InputFloat2("##val", &value.x)) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			else if constexpr (std::is_same_v<T, glm::vec3>) {
				if (ImGui::InputFloat3("##val", &value.x)) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
				}
			}
			}, prop.value);

		ImGui::PopID();
	}
}

void ScriptComponent::RunOnLoad() {
	CheckForFileChanges();
	EnsureLoaded();
	if (!loaded || onLoadRan) return;
	onLoadRan = true;

	py::gil_scoped_acquire gil;
	try {
		if (py::hasattr(scriptInstance, "OnLoad"))
			scriptInstance.attr("OnLoad")();
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: error in OnLoad() for {}: {}").Format(sourcePath, e.what());
		loaded = false;
		loadFailed = true;
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