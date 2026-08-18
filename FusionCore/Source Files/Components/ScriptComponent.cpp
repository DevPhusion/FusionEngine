#include "../../Header Files/Components/ScriptComponent.h"
#include "../../Header Files/Core/Scripting/PyBindings.h"
#include "../../Header Files/Core/EngineManager.h"
#include "../../Header Files/Core/ObjectManager.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace py = pybind11;

namespace {
	bool MatchesFileFilter(const std::string& virtualPath, const std::string& filterPattern) {
		if (filterPattern.empty() || filterPattern == "*.*" || filterPattern == "*") return true;

		std::string ext = std::filesystem::path(virtualPath).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		std::stringstream ss(filterPattern);
		std::string token;
		while (std::getline(ss, token, ';')) {
			if (!token.empty() && token[0] == '*') token.erase(0, 1);
			std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (token == ext) return true;
		}
		return false;
	}

	void WriteExportedValue(BinaryWriter& w, const ExportedValue& value) {
		w.Write(static_cast<int>(value.index()));
		std::visit([&](auto&& v) {
			using T = std::decay_t<decltype(v)>;
			if constexpr (std::is_same_v<T, std::string>) {
				w.WriteString(v);
			}
			else if constexpr (std::is_same_v<T, glm::vec2>) {
				w.Write(v.x); w.Write(v.y);
			}
			else if constexpr (std::is_same_v<T, glm::vec4>) {
				w.Write(v.x); w.Write(v.y); w.Write(v.z); w.Write(v.w);
			}
			else if constexpr (std::is_same_v<T, ObjectRef>) {
				w.Write(v.id);
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
		case 6: {
			float x = r.Read<float>();
			float y = r.Read<float>();
			float z = r.Read<float>();
			float w = r.Read<float>();
			return glm::vec4(x, y, z, w);
		}
		case 7: {
			ObjectRef ref;
			ref.id = r.Read<uint64_t>();
			ref.ptr = nullptr;
			return ref;
		}
		default: return std::string();
		}
	}
}

ScriptComponent::ScriptComponent(Object* parent, std::string sourcePath) : ComponentBase<ScriptComponent>(parent) {
	SetSourcePath(sourcePath);
	this->Name = "Script Component";
}

void ScriptComponent::Deactivate() {
	if (!isActive) return;

	if (loaded) {
		pendingExportedValues = exportedProperties;
	}
	Unload();

	isActive = false;
}

std::string ScriptComponent::GetDisplayName() {
	if (sourcePath.empty()) return "";
	return std::filesystem::path(sourcePath).stem().string();
}

void ScriptComponent::SetSourcePath(std::string path) {
	sourcePath = path;
	if (!sourcePath.empty()) {
		ScriptManager::getInstance().NotifyScriptAttached(sourcePath);
	}
	Unload();
}

void ScriptComponent::OnDelete() {
	Deactivate();
}

void ScriptComponent::CopyTo(Object* other) {
	ScriptComponent* target = other->GetComponent<ScriptComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<ScriptComponent>(other, ""));
		target = other->GetComponent<ScriptComponent>();
	}

	target->SetSourcePath(sourcePath);
	target->pendingExportedValues = loaded ? exportedProperties : pendingExportedValues;
	target->SetEnabled(Enabled);
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

void ScriptComponent::PostLoad() {
	ResolveObjectReferences(pendingExportedValues);

	if (loaded) {
		ResolveObjectReferences(exportedProperties);
		for (auto& prop : exportedProperties) {
			if (std::holds_alternative<ObjectRef>(prop.value)) {
				UnregisterObjectDeleteCallback(prop);
				RegisterObjectDeleteCallback(prop);
				SetInstanceAttrFromVariant(prop.name, prop.value);
			}
		}
	}
}

void ScriptComponent::ResolveObjectReferences(std::vector<ExportedProperty>& props) {
	for (auto& prop : props) {
		if (!std::holds_alternative<ObjectRef>(prop.value)) continue;
		ObjectRef& ref = std::get<ObjectRef>(prop.value);
		ref.ptr = ref.id != 0 ? ObjectManager::getInstance().FindObjectById(ref.id) : nullptr;
	}
}

void ScriptComponent::RegisterObjectDeleteCallback(ExportedProperty& prop) {
	if (!std::holds_alternative<ObjectRef>(prop.value)) return;
	ObjectRef& ref = std::get<ObjectRef>(prop.value);
	if (ref.ptr == nullptr) return;

	std::string name = prop.name;
	uint64_t expectedId = ref.id;
	prop.objectRefDeleteCallbackId = ref.ptr->AddOnDeleteCallback([this, name, expectedId]() {
		OnExportedObjectDeleted(name, expectedId);
		});
}

void ScriptComponent::UnregisterObjectDeleteCallback(ExportedProperty& prop) {
	if (!std::holds_alternative<ObjectRef>(prop.value)) return;
	ObjectRef& ref = std::get<ObjectRef>(prop.value);
	if (ref.ptr == nullptr || prop.objectRefDeleteCallbackId == -1) return;

	ref.ptr->RemoveOnDeleteCallback(prop.objectRefDeleteCallbackId);
	prop.objectRefDeleteCallbackId = -1;
}

void ScriptComponent::OnExportedObjectDeleted(const std::string& name, uint64_t expectedId) {
	auto it = std::find_if(exportedProperties.begin(), exportedProperties.end(),
		[&](ExportedProperty& p) { return p.name == name; });
	if (it == exportedProperties.end()) return;
	if (!std::holds_alternative<ObjectRef>(it->value)) return;

	ObjectRef& ref = std::get<ObjectRef>(it->value);
	if (ref.id != expectedId) return;

	ref.ptr = nullptr;
	ref.id = 0;
	it->objectRefDeleteCallbackId = -1;

	SetInstanceAttrFromVariant(it->name, it->value);
}

void ScriptComponent::Unload() {
	if (scriptInstance) {
		py::gil_scoped_acquire gil;
		scriptInstance = py::object();
	}
	for (auto& prop : exportedProperties) {
		UnregisterObjectDeleteCallback(prop);
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

	if (Py_IsInitialized() && !sourcePath.empty()) {
		py::gil_scoped_acquire gil;
		std::string moduleName = VirtualPathToModuleName(sourcePath);

		py::object sysModule = py::module_::import("sys");
		py::dict sysModules = sysModule.attr("modules");

		if (sysModules.contains(moduleName)) {
			sysModules.attr("pop")(moduleName, py::none());
		}
	}

	Unload();
	EnsureLoaded();

	if (!loaded) {
		Console::PrintError("ScriptComponent: reload failed for {} — keeping previous instance unloaded").Format(sourcePath);
		pendingExportedValues = std::move(previousValues);
		EngineManager::getInstance().SceneChangeEvent();
		return;
	}

	for (auto& prop : exportedProperties) {
		auto it = std::find_if(previousValues.begin(), previousValues.end(),
			[&](const ExportedProperty& p) { return p.name == prop.name; });

		if (it != previousValues.end() && it->value.index() == prop.value.index()) {
			UnregisterObjectDeleteCallback(prop);
			prop.value = it->value;
			RegisterObjectDeleteCallback(prop);
			SetInstanceAttrFromVariant(prop.name, prop.value);
		}
	}

	EngineManager::getInstance().SceneChangeEvent();
}

void ScriptComponent::EnsureLoaded() {
	if (loaded || loadFailed) return;
	if (!isActive) return;
	loadFailed = true;

	if (sourcePath.empty()) return;
	if (!Py_IsInitialized()) {
		Console::PrintError("ScriptComponent: Python backend isn't ready yet; cannot load {}").Format(sourcePath);
		return;
	}

	std::filesystem::path absPath = FileManager::getInstance().VirtualToAbsolute(sourcePath);
	std::error_code ec;
	bool existsOnDisk = std::filesystem::exists(absPath, ec);
	bool existsInPackage = ExportPackageReader::getInstance().Get(sourcePath) != nullptr;

	if (!existsOnDisk && !existsInPackage) {
		Console::PrintError("ScriptComponent: script file not found: {}").Format(sourcePath);
		return;
	}

	std::error_code ecStat;
	lastWriteTime = existsOnDisk
		? std::filesystem::last_write_time(absPath, ecStat)
		: std::filesystem::file_time_type{};

	py::gil_scoped_acquire gil;

	try {
		py::object foundClass = ImportScriptClass(sourcePath);

		scriptInstance = foundClass();

		try {
			scriptInstance.cast<ScriptBase&>().parent = this->parent;
		}
		catch (const py::cast_error&) {
			Console::PrintError("ScriptComponent: class in {} does not properly inherit from Script").Format(absPath.string());
			scriptInstance = py::object();
			return;
		}

		ScanExportedProperties();
		loaded = true;
		loadFailed = false;
	}
	catch (const py::error_already_set& e) {
		Console::PrintError("ScriptComponent: failed to load script {}: {}").Format(absPath.string(), e.what());
		scriptInstance = py::object();
	}
	catch (const std::exception& e) {
		Console::PrintError("ScriptComponent: failed to load script {}: {}").Format(absPath.string(), e.what());
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
		ExportMarker marker = attrValue.cast<ExportMarker>();
		py::object defaultValue = marker.value;

		ExportedProperty prop;
		prop.name = attrName;
		prop.displayType = marker.type;
		prop.prefix = marker.prefix;
		prop.suffix = marker.suffix;
		prop.min = marker.min;
		prop.max = marker.max;
		prop.fileFilter = marker.fileFilter;

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
		else if (py::isinstance<glm::vec4>(defaultValue)) {
			prop.value = defaultValue.cast<glm::vec4>();
		}
		else if (py::isinstance<glm::vec3>(defaultValue)) {
			prop.value = defaultValue.cast<glm::vec3>();
		}
		else if (py::isinstance<glm::vec2>(defaultValue)) {
			prop.value = defaultValue.cast<glm::vec2>();
		}
		else if (py::isinstance<Object>(defaultValue)) {
			Object* obj = defaultValue.cast<Object*>();
			prop.value = ObjectRef{ obj, obj ? obj->id : 0 };
			RegisterObjectDeleteCallback(prop);
		}
		else if (PyType_Check(defaultValue.ptr()) && defaultValue.is(py::type::of<Object>())) {
			prop.value = ObjectRef{};
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
			UnregisterObjectDeleteCallback(prop);
			prop.value = it->value;
			RegisterObjectDeleteCallback(prop);
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
				if constexpr (std::is_same_v<T, ObjectRef>) {
					Object* obj = current.cast<Object*>();
					stored.ptr = obj;
					stored.id = obj ? obj->id : 0;
				}
				else {
					stored = current.cast<T>();
				}
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
		using T = std::decay_t<decltype(v)>;
		if constexpr (std::is_same_v<T, ObjectRef>) {
			scriptInstance.attr(name.c_str()) = py::cast(v.ptr, py::return_value_policy::reference);
		}
		else {
			scriptInstance.attr(name.c_str()) = py::cast(v);
		}
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
				if (prop.displayType == ExportType::File) {
					char displayBuf[256] = "None (click to choose...)";
					if (!value.empty()) {
#if defined(_MSC_VER)
						strcpy_s(displayBuf, value.c_str());
#else
						strncpy(displayBuf, value.c_str(), sizeof(displayBuf) - 1);
						displayBuf[sizeof(displayBuf) - 1] = '\0';
#endif
					}

					ImGui::InputText("##val", displayBuf, sizeof(displayBuf), ImGuiInputTextFlags_ReadOnly);
					if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
					if (ImGui::IsItemClicked()) {
						FileDialogOptions opts;
						opts.title = "Choose File";
						opts.filters = {
							{ prop.fileFilter == "*.fscene" ? "Scene Files" : "Files", prop.fileFilter },
							{ "All Files", "*.*" }
						};
						if (auto chosen = FileDialog::ShowOpenDialog(opts)) {
							std::string virtualPath = FileManager::getInstance().AbsoluteToVirtual(*chosen);
							EditorManager::getInstance().BeginEdit({ parent });
							value = virtualPath;
							SetInstanceAttrFromVariant(prop.name, prop.value);
							EditorManager::getInstance().EndEdit({ parent });
							EngineManager::getInstance().SceneChangeEvent();
						}
					}

					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FileManager::kResourceDragDropPayloadType)) {
							std::string virtualPath(static_cast<const char*>(payload->Data));
							FileManager& fm = FileManager::getInstance();

							if (fm.IsDirectory(virtualPath)) {
							}
							else if (!MatchesFileFilter(virtualPath, prop.fileFilter)) {
								Console::PrintWarning("Script export '" + prop.name +
									"': dropped file does not match the required extension (" + prop.fileFilter + ")");
							}
							else {
								EditorManager::getInstance().BeginEdit({ parent });
								value = virtualPath;
								SetInstanceAttrFromVariant(prop.name, prop.value);
								EditorManager::getInstance().EndEdit({ parent });
								EngineManager::getInstance().SceneChangeEvent();
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
				else {
					char buf[256];
					std::memset(buf, 0, sizeof(buf));
					std::size_t len = std::min(value.size(), sizeof(buf) - 1);
					std::memcpy(buf, value.data(), len);

					if (ImGui::InputText("##val", buf, sizeof(buf))) {
						if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
						value = std::string(buf);
						SetInstanceAttrFromVariant(prop.name, prop.value);
						EngineManager::getInstance().SceneChangeEvent();
					}
					if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
				}
			}
			else if constexpr (std::is_same_v<T, int>) {
				bool changed = false;
				std::string format = prop.prefix + "%d" + prop.suffix;
				switch (prop.displayType) {
				case ExportType::Slider:
					changed = ImGui::SliderInt("##val", &value, (int)prop.min, (int)prop.max, format.c_str());
					break;
				case ExportType::Drag:
					changed = ImGui::DragInt("##val", &value, 1.0f, (int)prop.min, (int)prop.max, format.c_str());
					break;
				default:
					changed = ImGui::InputInt("##val", &value);
					break;
				}
				if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
				if (changed) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EngineManager::getInstance().SceneChangeEvent();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
			}
			else if constexpr (std::is_same_v<T, float>) {
				bool changed = false;
				std::string format = prop.prefix + "%.3f" + prop.suffix;
				switch (prop.displayType) {
				case ExportType::Slider:
					changed = ImGui::SliderFloat("##val", &value, prop.min, prop.max, format.c_str());
					break;
				case ExportType::AngleSlider:
					changed = ImGui::SliderAngle("##val", &value, prop.min, prop.max);
					break;
				case ExportType::Drag:
					changed = ImGui::DragFloat("##val", &value, 1.0f, prop.min, prop.max, format.c_str());
					break;
				default:
					changed = ImGui::InputFloat("##val", &value, 0.0f, 0.0f, format.c_str());
					break;
				}
				if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
				if (changed) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EngineManager::getInstance().SceneChangeEvent();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
			}
			else if constexpr (std::is_same_v<T, bool>) {
				if (ImGui::Checkbox("##val", &value)) {
					EditorManager::getInstance().BeginEdit({ parent });
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EditorManager::getInstance().EndEdit({ parent });
					EngineManager::getInstance().SceneChangeEvent();
				}
			}
			else if constexpr (std::is_same_v<T, glm::vec2>) {
				if (ImGui::InputFloat2("##val", &value.x)) {
					if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EngineManager::getInstance().SceneChangeEvent();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
			}
			else if constexpr (std::is_same_v<T, glm::vec3>) {
				bool changed = false;
				switch (prop.displayType) {
				case ExportType::ColorEdit:   changed = ImGui::ColorEdit3("##val", &value.x); break;
				case ExportType::ColorPicker: changed = ImGui::ColorPicker3("##val", &value.x); break;
				default:                      changed = ImGui::InputFloat3("##val", &value.x); break;
				}
				if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
				if (changed) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EngineManager::getInstance().SceneChangeEvent();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
			}
			else if constexpr (std::is_same_v<T, glm::vec4>) {
				bool changed = false;
				switch (prop.displayType) {
				case ExportType::ColorEdit:   changed = ImGui::ColorEdit4("##val", &value.x); break;
				case ExportType::ColorPicker: changed = ImGui::ColorPicker4("##val", &value.x); break;
				default:                      changed = ImGui::InputFloat4("##val", &value.x); break;
				}
				if (ImGui::IsItemActivated()) EditorManager::getInstance().BeginEdit({ parent });
				if (changed) {
					SetInstanceAttrFromVariant(prop.name, prop.value);
					EngineManager::getInstance().SceneChangeEvent();
				}
				if (ImGui::IsItemDeactivatedAfterEdit()) EditorManager::getInstance().EndEdit({ parent });
			}
			else if constexpr (std::is_same_v<T, ObjectRef>) {
				char nameBuf[128];
				std::string label = value.ptr ? value.ptr->name : std::string("None (Click to choose...)");
				std::snprintf(nameBuf, sizeof(nameBuf), "%s", label.c_str());

				ImGui::InputText("##val", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_ReadOnly);
				if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsItemClicked()) ImGui::OpenPopup("##ExportObjectPicker");

				if (ImGui::BeginPopup("##ExportObjectPicker")) {
					ImGui::TextDisabled("Select Object");
					ImGui::Separator();

					bool changed = false;

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
					if (ImGui::Selectable("None")) {
						EditorManager::getInstance().BeginEdit({ parent });
						UnregisterObjectDeleteCallback(prop);
						value.ptr = nullptr;
						value.id = 0;
						changed = true;
					}
					ImGui::PopStyleColor();
					ImGui::Separator();

					for (auto& objPtr : ObjectManager::getInstance().allObjects) {
						Object* candidate = objPtr.get();
						if (ImGui::Selectable(candidate->name.c_str(), candidate == value.ptr)) {
							EditorManager::getInstance().BeginEdit({ parent });
							UnregisterObjectDeleteCallback(prop);
							value.ptr = candidate;
							value.id = candidate->id;
							RegisterObjectDeleteCallback(prop);
							changed = true;
						}
					}
					ImGui::EndPopup();
					if (changed) {
						SetInstanceAttrFromVariant(prop.name, prop.value);
						EngineManager::getInstance().SceneChangeEvent();
						EditorManager::getInstance().EndEdit({ parent });
					}
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