#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/Editor/Windows/Console.h"
#include "../../../Header Files/Components/TransformComponent.h"
#include "../../../Header Files/Components/EditorRenderComponent.h"
#include "../../../Header Files/Components/MouseInteractComponent.h"
#include "../../../Header Files/Components/ScriptComponent.h"
#include "../../../Header Files/Core/Files/FileManager.h"
#include "../../../Header Files/Core/ObjectManager.h"
#include <pybind11/stl.h>
#include <pybind11/operators.h>  
#include <glm/glm.hpp>
#include <sstream>
#include <numbers>

namespace {
	void RegisterMathBindings(py::module_& m) {
		py::class_<glm::vec2>(m, "Vector2")
			.def(py::init<>())
			.def(py::init<float, float>(), py::arg("x"), py::arg("y"))
			.def(py::init<float>(), py::arg("scalar"))
			.def_readwrite("x", &glm::vec2::x)
			.def_readwrite("y", &glm::vec2::y)

			.def(py::self + py::self)
			.def(py::self - py::self)
			.def(py::self * py::self)
			.def(py::self * float())
			.def(float() * py::self)
			.def(py::self / float())
			.def(-py::self)
			.def(py::self == py::self)
			.def(py::self != py::self)

			.def("length", [](const glm::vec2& v) { return glm::length(v);})
			.def("normalize", [](const glm::vec2& v) { return glm::normalize(v); })
			.def("dot", [](const glm::vec2& a, const glm::vec2& b) {return glm::dot(a, b);})
			.def_static("lerp", [](const glm::vec2& a, const glm::vec2& b, float t) {
			return glm::mix(a, b, t);
				}, py::arg("a"), py::arg("b"), py::arg("t"),
					"Linearly interpolate between a and b (t=0 -> a, t=1 -> b), e.g. Vector2.lerp(start, end, 0.5)")
			.def("__repr__", [](const glm::vec2& v) {
			std::ostringstream ss;
			ss << "Vector2(" << v.x << ", " << v.y << ")";
			return ss.str();
				})
			.def("__len__", [](const glm::vec2&) { return 2; })
			.def("__getitem__", [](const glm::vec2& v, int i) {
			if (i < 0 || i > 1) throw py::index_error();
			return v[i];
				})
			.def("__setitem__", [](glm::vec2& v, int i, float val) {
			if (i < 0 || i > 1) throw py::index_error();
			v[i] = val;
				});

		py::class_<glm::vec3>(m, "Vector3")
			.def(py::init<>())
			.def(py::init<float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z"))
			.def(py::init<float>(), py::arg("scalar"))
			.def_readwrite("x", &glm::vec3::x)
			.def_readwrite("y", &glm::vec3::y)
			.def_readwrite("z", &glm::vec3::z)

			.def(py::self + py::self)
			.def(py::self - py::self)
			.def(py::self * py::self)
			.def(py::self * float())
			.def(float() * py::self)
			.def(py::self / float())
			.def(-py::self)
			.def(py::self == py::self)
			.def(py::self != py::self)

			.def("length", [](const glm::vec3& v) { return glm::length(v); })
			.def("normalized", [](const glm::vec3& v) { return glm::normalize(v); })
			.def("dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); })
			.def("cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); })
			.def_static("lerp", [](const glm::vec3& a, const glm::vec3& b, float t) {
			return glm::mix(a, b, t);
				}, py::arg("a"), py::arg("b"), py::arg("t"),
					"Linearly interpolate between a and b (t=0 -> a, t=1 -> b), e.g. Vector3.lerp(start, end, 0.5)")

			.def("__repr__", [](const glm::vec3& v) {
			std::ostringstream ss;
			ss << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
			return ss.str();
				})
			.def("__len__", [](const glm::vec3&) { return 3; })
			.def("__getitem__", [](const glm::vec3& v, int i) {
			if (i < 0 || i > 2) throw py::index_error();
			return v[i];
				})
			.def("__setitem__", [](glm::vec3& v, int i, float val) {
			if (i < 0 || i > 2) throw py::index_error();
			v[i] = val;
				});

		py::class_<glm::vec4>(m, "Vector4")
			.def(py::init<>())
			.def(py::init<float, float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"))
			.def(py::init<float>(), py::arg("scalar"))
			.def_readwrite("x", &glm::vec4::x)
			.def_readwrite("y", &glm::vec4::y)
			.def_readwrite("z", &glm::vec4::z)
			.def_readwrite("w", &glm::vec4::w)

			.def_readwrite("r", &glm::vec4::r)
			.def_readwrite("g", &glm::vec4::g)
			.def_readwrite("b", &glm::vec4::b)
			.def_readwrite("a", &glm::vec4::a)

			.def(py::self + py::self)
			.def(py::self - py::self)
			.def(py::self * py::self)
			.def(py::self * float())
			.def(float() * py::self)
			.def(py::self / float())
			.def(-py::self)
			.def(py::self == py::self)
			.def(py::self != py::self)

			.def("length", [](const glm::vec4& v) { return glm::length(v); })
			.def("normalized", [](const glm::vec4& v) { return glm::normalize(v); })
			.def("dot", [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); })
			.def_static("lerp", [](const glm::vec4& a, const glm::vec4& b, float t) {
			return glm::mix(a, b, t);
				}, py::arg("a"), py::arg("b"), py::arg("t"),
					"Linearly interpolate between a and b (t=0 -> a, t=1 -> b), e.g. Vector4.lerp(start, end, 0.5)")

			.def("__repr__", [](const glm::vec4& v) {
			std::ostringstream ss;
			ss << "Vector4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
			return ss.str();
				})
			.def("__len__", [](const glm::vec4&) { return 4; })
			.def("__getitem__", [](const glm::vec4& v, int i) {
			if (i < 0 || i > 3) throw py::index_error();
			return v[i];
				})
			.def("__setitem__", [](glm::vec4& v, int i, float val) {
			if (i < 0 || i > 3) throw py::index_error();
			v[i] = val;
				});

		m.def("lerp", [](float a, float b, float t) {
			return glm::mix(a, b, t);
			}, py::arg("a"), py::arg("b"), py::arg("t"),
				"Linearly interpolate between two floats (t=0 -> a, t=1 -> b)");
	}

	void RegisterInputBindings(py::module_& m) {
		py::class_<InputManager> inputClass(m, "Input");

		inputClass
			.def_static("is_key_pressed", [](int key) {
			return InputManager::getInstance().IsKeyPressed(key);
				}, py::arg("key"),
					"True every frame the key is held down")
			.def_static("is_key_just_pressed", [](int key) {
			return InputManager::getInstance().IsKeyJustPressed(key);
				}, py::arg("key"),
					"True only on the frame the key was pressed")
			.def_static("is_key_released", [](int key) {
			return InputManager::getInstance().IsKeyReleased(key);
				}, py::arg("key"),
					"True only on the frame the key was released")

			.def_static("is_mouse_button_pressed", [](int button) {
			return InputManager::getInstance().IsMouseButtonPressed(button);
				}, py::arg("button"))
			.def_static("is_mouse_button_just_pressed", [](int button) {
			return InputManager::getInstance().IsMouseButtonJustPressed(button);
				}, py::arg("button"))
			.def_static("is_mouse_button_released", [](int button) {
			return InputManager::getInstance().IsMouseButtonReleased(button);
				}, py::arg("button"))

			.def_static("on_key_pressed", [](int key, py::function func) {
			return InputManager::getInstance().OnKeyPressed(key, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_key_pressed callback error: ") + e.what());
				}
				});
				}, py::arg("key"), py::arg("callback"),
					"Fires every frame while the key is held. Returns an id usable with remove_key_pressed_callback().")

			.def_static("on_key_just_pressed", [](int key, py::function func) {
			return InputManager::getInstance().OnKeyJustPressed(key, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_key_just_pressed callback error: ") + e.what());
				}
				});
				}, py::arg("key"), py::arg("callback"))

			.def_static("on_key_released", [](int key, py::function func) {
			return InputManager::getInstance().OnKeyReleased(key, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_key_released callback error: ") + e.what());
				}
				});
				}, py::arg("key"), py::arg("callback"))

			.def_static("on_mouse_button_pressed", [](int button, py::function func) {
			return InputManager::getInstance().OnMouseButtonPressed(button, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_mouse_button_pressed callback error: ") + e.what());
				}
				});
				}, py::arg("button"), py::arg("callback"))

			.def_static("on_mouse_button_just_pressed", [](int button, py::function func) {
			return InputManager::getInstance().OnMouseButtonJustPressed(button, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_mouse_button_just_pressed callback error: ") + e.what());
				}
				});
				}, py::arg("button"), py::arg("callback"))

			.def_static("on_mouse_button_released", [](int button, py::function func) {
			return InputManager::getInstance().OnMouseButtonReleased(button, [func]() {
				py::gil_scoped_acquire gil;
				try { func(); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("on_mouse_button_released callback error: ") + e.what());
				}
				});
				}, py::arg("button"), py::arg("callback"))

			.def_static("remove_key_pressed_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveKeyPressedCallback(id);
				}, py::arg("id"))
			.def_static("remove_key_just_pressed_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveKeyJustPressedCallback(id);
				}, py::arg("id"))
			.def_static("remove_key_released_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveKeyReleasedCallback(id);
				}, py::arg("id"))
			.def_static("remove_mouse_button_pressed_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveMouseButtonPressedCallback(id);
				}, py::arg("id"))
			.def_static("remove_mouse_button_just_pressed_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveMouseButtonJustPressedCallback(id);
				}, py::arg("id"))
			.def_static("remove_mouse_button_released_callback", [](std::pair<int, int> id) {
			InputManager::getInstance().RemoveMouseButtonReleasedCallback(id);
				}, py::arg("id"));

		py::module_ keyMod = m.def_submodule("Key", "Common GLFW key codes");
		keyMod.attr("SPACE") = static_cast<int>(GLFW_KEY_SPACE);
		keyMod.attr("ENTER") = static_cast<int>(GLFW_KEY_ENTER);
		keyMod.attr("ESCAPE") = static_cast<int>(GLFW_KEY_ESCAPE);
		keyMod.attr("LEFT_SHIFT") = static_cast<int>(GLFW_KEY_LEFT_SHIFT);
		keyMod.attr("LEFT_CONTROL") = static_cast<int>(GLFW_KEY_LEFT_CONTROL);
		keyMod.attr("LEFT_ALT") = static_cast<int>(GLFW_KEY_LEFT_ALT);
		keyMod.attr("UP") = static_cast<int>(GLFW_KEY_UP);
		keyMod.attr("DOWN") = static_cast<int>(GLFW_KEY_DOWN);
		keyMod.attr("LEFT") = static_cast<int>(GLFW_KEY_LEFT);
		keyMod.attr("RIGHT") = static_cast<int>(GLFW_KEY_RIGHT);
		for (char c = 'A'; c <= 'Z'; ++c) {
			keyMod.attr(std::string(1, c).c_str()) = static_cast<int>(GLFW_KEY_A + (c - 'A'));
		}
		for (char c = '0'; c <= '9'; ++c) {
			keyMod.attr((std::string("NUM_") + c).c_str()) = static_cast<int>(GLFW_KEY_0 + (c - '0'));
		}

		py::module_ mouseMod = m.def_submodule("Mouse", "Mouse button codes");
		mouseMod.attr("LEFT") = static_cast<int>(GLFW_MOUSE_BUTTON_LEFT);
		mouseMod.attr("RIGHT") = static_cast<int>(GLFW_MOUSE_BUTTON_RIGHT);
		mouseMod.attr("MIDDLE") = static_cast<int>(GLFW_MOUSE_BUTTON_MIDDLE);
	}

	std::unordered_map<PyObject*, std::function<py::object(Object*)>>& ComponentRegistry() {
		static std::unordered_map<PyObject*, std::function<py::object(Object*)>> registry;
		return registry;
	}

	std::unordered_map<PyObject*, std::function<void(Object*)>>& ComponentRemoverRegistry() {
		static std::unordered_map<PyObject*, std::function<void(Object*)>> registry;
		return registry;
	}

	std::unordered_map<PyObject*, std::function<py::object(Object&)>>& ComponentAdderRegistry() {
		static std::unordered_map<PyObject*, std::function<py::object(Object&)>> registry;
		return registry;
	}

	void RegisterScriptClassAsComponentType(const py::object& classObj, const std::string& virtualPath) {
		PyObject* key = classObj.ptr();
		if (ComponentRegistry().count(key)) return; 

		ComponentRegistry()[key] = [key](Object* obj) -> py::object {
			py::gil_scoped_acquire gil;
			for (auto& comp : obj->components) {
				auto* script = dynamic_cast<ScriptComponent*>(comp.get());
				if (!script || !script->HasInstance()) continue;
				if (script->GetInstance().attr("__class__").ptr() == key) {
					return script->GetInstance();
				}
			}
			return py::none();
			};

		ComponentRemoverRegistry()[key] = [key](Object* obj) {
			py::gil_scoped_acquire gil;
			for (int i = 0; i < static_cast<int>(obj->components.size()); i++) {
				auto* script = dynamic_cast<ScriptComponent*>(obj->components[i].get());
				if (!script || !script->HasInstance()) continue;
				if (script->GetInstance().attr("__class__").ptr() == key) {
					obj->RemoveComponent(i);
					return;
				}
			}
			};

		ComponentAdderRegistry()[key] = [virtualPath](Object& obj) -> py::object {
			auto comp = std::make_unique<ScriptComponent>(&obj, virtualPath);
			ScriptComponent* raw = comp.get();
			obj.AddComponent(std::move(comp));

			raw->EnsureLoaded();
			if (!raw->IsLoaded()) {
				throw py::value_error("add_component: failed to load script '" + virtualPath + "'");
			}
			return raw->GetInstance();
			};
	}

	bool TryAutoRegisterScriptClass(const py::object& componentClass) {
		py::gil_scoped_acquire gil;

		py::object fusionModule = py::module_::import("fusion");
		py::object scriptBaseClass = fusionModule.attr("Script");

		if (!PyType_Check(componentClass.ptr())) return false;
		if (componentClass.is(scriptBaseClass)) return false;

		int isSub = PyObject_IsSubclass(componentClass.ptr(), scriptBaseClass.ptr());
		if (isSub != 1) { if (isSub < 0) PyErr_Clear(); return false; }

		std::string absPathStr;
		try {
			py::object inspectModule = py::module_::import("inspect");
			absPathStr = inspectModule.attr("getfile")(componentClass).cast<std::string>();
		}
		catch (const py::error_already_set&) {
			PyErr_Clear();
			return false;
		}

		std::string virtualPath = FileManager::getInstance().AbsoluteToVirtual(absPathStr);
		if (virtualPath.empty() || virtualPath == "res://") return false; 

		RegisterScriptClassAsComponentType(componentClass, virtualPath);
		return true;
	}

	template <typename Registry>
	auto FindOrAutoRegister(Registry& registry, const py::object& componentClass) {
		auto it = registry.find(componentClass.ptr());
		if (it == registry.end() && TryAutoRegisterScriptClass(componentClass)) {
			it = registry.find(componentClass.ptr());
		}
		return it;
	}

	py::object GetComponentByPythonType(Object* parent, const py::object& componentClass) {
		if (!parent) return py::none();

		auto& registry = ComponentRegistry();
		auto it = FindOrAutoRegister(registry, componentClass);
		if (it == registry.end()) {
			throw py::type_error(
				"get_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}
		return it->second(parent);
	}

	void RemoveComponentFromObject(Object& obj, py::object componentClass) {
		auto& registry = ComponentRemoverRegistry();
		auto it = FindOrAutoRegister(registry, componentClass);
		if (it == registry.end()) {
			throw py::type_error(
				"remove_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}
		it->second(&obj);
	}

	bool HasComponentOnObject(Object* obj, const py::object& componentClass) {
		if (!obj) return false;

		auto& registry = ComponentRegistry();
		auto it = FindOrAutoRegister(registry, componentClass);
		if (it == registry.end()) {
			throw py::type_error(
				"has_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}
		return !it->second(obj).is_none();
	}

	py::object AddComponentToObject(Object& obj, py::object componentClass) {
		auto& registry = ComponentAdderRegistry();
		auto it = FindOrAutoRegister(registry, componentClass);
		if (it == registry.end()) {
			throw py::type_error(
				"add_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}

		if (HasComponentOnObject(&obj, componentClass)) {
			throw py::value_error(
				"add_component: Object '" + obj.name + "' already has a '" +
				py::str(componentClass).cast<std::string>() + "'");
		}

		return it->second(obj);
	}

	Object* AddObjectToScene(Object* obj, Object* parent) {
		if (!obj) {
			throw py::value_error("add_object: obj cannot be None");
		}
		if (obj->addedToScene) {
			throw py::value_error("add_object: object has already been added to the scene");
		}
		return ObjectManager::getInstance().AddExistingObject(std::unique_ptr<Object>(obj), parent);
	}

	void RemoveObjectFromScene(Object* obj) {
		if (!obj) {
			throw py::value_error("remove_object: obj cannot be None");
		}
		ObjectManager::getInstance().QueueRemoveObject(obj);
	}

	template <typename T, typename PyClass>
	void EnableGetComponent(PyClass& cls) {
		cls.def("get_component", [](T& self, py::object componentClass) {
			return GetComponentByPythonType(self.parent, componentClass);
			}, py::arg("component_class"),
				"Look up a component on this Object");
	}

	template <typename T, typename PyClass>
	void EnableAddComponent(PyClass& cls) {
		cls.def("add_component", [](T& self, py::object componentClass) {
			if (!self.parent) {
				throw py::value_error("add_component: component has no owning Object");
			}
			return AddComponentToObject(*self.parent, componentClass);
			}, py::arg("component_class"),
				"Attach a new component of the given type to this component's owning Object, "
				"e.g. render = self.add_component(RenderComponent)");
	}

	template <typename T, typename PyClass>
	void EnableRemoveComponent(PyClass& cls) {
		cls.def("remove_component", [](T& self, py::object componentClass) {
			if (!self.parent) {
				throw py::value_error("remove_component: component has no owning Object");
			}
			RemoveComponentFromObject(*self.parent, componentClass);
			}, py::arg("component_class"),
				"Remove a component of the given type from this component's owning Object");
	}

	template <typename T, typename PyClass>
	void EnableHasComponent(PyClass& cls) {
		cls.def("has_component", [](T& self, py::object componentClass) {
			return HasComponentOnObject(self.parent, componentClass);
			}, py::arg("component_class"),
				"Check whether this component's owning Object has a component of the given type");
	}

	template <typename T, typename PyClass>
	void EnableGetOwner(PyClass& cls) {
		cls.def_property_readonly("owner", [](T& self) -> Object* {
			return self.parent;
			}, py::return_value_policy::reference,
			"The Object that owns this component")
			.def("get_owner", [](T& self) -> Object* {
			return self.parent;
				}, py::return_value_policy::reference);
	}

	template <typename T, typename PyClass>
	void EnableAddObject(PyClass& cls) {
		cls.def("add_object", [](T&, Object* obj, Object* parent) {
			return AddObjectToScene(obj, parent);
			}, py::arg("obj"), py::arg("parent") = nullptr,
				py::return_value_policy::reference,
				"Add a newly created Object to the scene, optionally parented to another Object");
	}

	template <typename T, typename PyClass>
	void EnableRemoveObject(PyClass& cls) {
		cls.def("remove_object", [](T&, Object* obj) {
			RemoveObjectFromScene(obj);
			}, py::arg("obj"),
				"Remove an object from the scene");
	}

	template <typename T, typename PyClass>
	void EnableAddChild(PyClass& cls) {
		cls.def("add_child", [](T& self, Object* obj) {
			if (!self.parent) {
				throw py::value_error("add_child: component has no owning Object");
			}
			return AddObjectToScene(obj, self.parent);
			}, py::arg("obj"),
				py::return_value_policy::reference,
				"Add obj as a child of this component's owning Object");
	}

	template <typename T>
	void RegisterComponentGetter(py::object pyClass) {
		ComponentRegistry()[pyClass.ptr()] = [](Object* obj) -> py::object {
			T* comp = obj->GetComponent<T>();
			if (!comp) return py::none();
			return py::cast(comp, py::return_value_policy::reference);
			};
	}

	template <typename T>
	void RegisterComponentRemover(py::object pyClass) {
		ComponentRemoverRegistry()[pyClass.ptr()] = [](Object* obj) {
			obj->RemoveComponent<T>();
			};
	}

	template <typename T>
	void RegisterComponentAdder(py::object pyClass, std::function<std::unique_ptr<T>(Object&)> factory) {
		ComponentAdderRegistry()[pyClass.ptr()] = [factory](Object& obj) -> py::object {
			auto comp = factory(obj);
			T* raw = comp.get();
			obj.RegisterComponentPointer(raw);
			obj.AddComponent(std::move(comp));
			return py::cast(raw, py::return_value_policy::reference);
			};
	}

	void RegisterScriptBindings(py::module_& m) {
		auto scriptClass = py::class_<ScriptBase, std::shared_ptr<ScriptBase>>(m, "Script")
			.def(py::init<>());

		EnableGetComponent<ScriptBase>(scriptClass);
		EnableGetOwner<ScriptBase>(scriptClass);
		EnableHasComponent<ScriptBase>(scriptClass);
		EnableAddComponent<ScriptBase>(scriptClass);
		EnableRemoveComponent<ScriptBase>(scriptClass);
		EnableAddObject<ScriptBase>(scriptClass);
		EnableAddChild<ScriptBase>(scriptClass);
		EnableRemoveObject<ScriptBase>(scriptClass);

		py::enum_<ExportType>(m, "ExportType")
			.value("Default", ExportType::Default)
			.value("Slider", ExportType::Slider)
			.value("AngleSlider", ExportType::AngleSlider)
			.value("ColorEdit", ExportType::ColorEdit)
			.value("ColorPicker", ExportType::ColorPicker)
			.value("Drag", ExportType::Drag)
			.export_values();

		py::class_<ExportMarker>(m, "_ExportMarker")
			.def(py::init<py::object, ExportType, std::string, std::string, float, float>(),
				py::arg("value"), py::arg("type") = ExportType::Default,
				py::arg("prefix") = "", py::arg("suffix") = "",
				py::arg("min") = 0.0f, py::arg("max") = 1.0f)
			.def_readonly("value", &ExportMarker::value)
			.def_readonly("type", &ExportMarker::type)
			.def_readonly("prefix", &ExportMarker::prefix)
			.def_readonly("suffix", &ExportMarker::suffix)
			.def_readonly("min", &ExportMarker::min)
			.def_readonly("max", &ExportMarker::max);

		m.def("export", [](py::object value, ExportType type, std::string prefix,
			std::string suffix, float min, float max) {
				return ExportMarker{ value, type, prefix, suffix, min, max };
			},
			py::arg("value"), py::arg("type") = ExportType::Default,
			py::arg("prefix") = "", py::arg("suffix") = "",
			py::arg("min") = 0.0f, py::arg("max") = 1.0f,
			"Mark a script attribute as editable in the inspector, e.g.\n"
			"  speed = export(200.0, ExportType.Slider, min=0.0, max=500.0)");

		m.def("get_script", &ImportScriptClass, py::arg("path"),
			"Import and return the Script subclass at the given res:// path. Only needed "
			"for genuinely dynamic/data-driven loading — for normal use, just import the "
			"class directly, e.g. `from scripts.enemy import Enemy`.");
	}

	void RegisterConsoleBindings(py::module_& m) {
		py::class_<Console>(m, "Console")
			.def_static("Print", [](const py::object& obj) {
			Console::AddMessage(Console::MessageType::Info, py::str(obj).cast<std::string>());
				}, py::arg("value"))
			.def_static("PrintWarning", [](const py::object& obj) {
			Console::AddMessage(Console::MessageType::Warning, py::str(obj).cast<std::string>());
				}, py::arg("value"))
			.def_static("PrintError", [](const py::object& obj) {
			Console::AddMessage(Console::MessageType::Error, py::str(obj).cast<std::string>());
				}, py::arg("value"));
	}

	void RegisterShapeBindings(py::module_& m) {
		py::class_<PolygonShape>(m, "PolygonShape")
			.def(py::init<>())
			.def(py::init([](std::vector<glm::vec3> points) {
			PolygonShape s;

			if (points.size() < 3) {
				throw py::value_error("PolygonShape requires at least 3 points");
			}

			float minX = points[0].x, maxX = points[0].x;
			float minY = points[0].y, maxY = points[0].y;
			for (auto& p : points) {
				minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
			}
			float rangeX = (maxX - minX) > 1e-6f ? (maxX - minX) : 1.0f;
			float rangeY = (maxY - minY) > 1e-6f ? (maxY - minY) : 1.0f;

			s.vertices.reserve(points.size() * 5);
			for (auto& p : points) {
				float u = (p.x - minX) / rangeX;
				float v = (p.y - minY) / rangeY;
				s.vertices.insert(s.vertices.end(), { p.x, p.y, p.z, u, v });
			}

			return s;
				}), py::arg("points"),
					"Build a polygon from a list of Vector3 points in local coordinates, "
					"e.g. [Vector3(0,0,0), Vector3(1,0,0), Vector3(0,1,0)]")
			.def_readwrite("vertices", &PolygonShape::vertices);

		py::class_<RectangleShape>(m, "RectangleShape")
			.def(py::init<>())
			.def(py::init([](glm::vec3 center, float width, float height) {
			RectangleShape s;
			s.center = center;
			s.width = width;
			s.height = height;
			return s;
				}), py::arg("center"), py::arg("width"), py::arg("height"))
			.def_readwrite("center", &RectangleShape::center)
			.def_readwrite("width", &RectangleShape::width)
			.def_readwrite("height", &RectangleShape::height);

		py::class_<CircleShape>(m, "CircleShape")
			.def(py::init<>())
			.def(py::init([](glm::vec3 center, float radius, int segments, int physicsSegments) {
			CircleShape s;
			s.center = center;
			s.radius = radius;
			s.segments = segments;
			s.physicsSegments = physicsSegments;
			return s;
				}), py::arg("center"), py::arg("radius"),
					py::arg("segments") = 30, py::arg("physics_segments") = 30)
			.def_readwrite("center", &CircleShape::center)
			.def_readwrite("radius", &CircleShape::radius)
			.def_readwrite("segments", &CircleShape::segments)
			.def_readwrite("physics_segments", &CircleShape::physicsSegments);
	}
	
	void RegisterPhysicsBindings(py::module_& m) {
		py::enum_<CollisionLayer>(m, "CollisionLayer",
			"Collision layer bit flags. Combine multiple with |, e.g. "
			"CollisionLayer.LAYER_1 | CollisionLayer.LAYER_3")
			.value("LAYER_1", CollisionLayer::LAYER_1)
			.value("LAYER_2", CollisionLayer::LAYER_2)
			.value("LAYER_3", CollisionLayer::LAYER_3)
			.value("LAYER_4", CollisionLayer::LAYER_4)
			.value("LAYER_5", CollisionLayer::LAYER_5)
			.value("LAYER_6", CollisionLayer::LAYER_6)
			.value("LAYER_7", CollisionLayer::LAYER_7)
			.value("LAYER_8", CollisionLayer::LAYER_8)
			.value("LAYER_9", CollisionLayer::LAYER_9)
			.value("LAYER_10", CollisionLayer::LAYER_10)
			.value("LAYER_11", CollisionLayer::LAYER_11)
			.value("LAYER_12", CollisionLayer::LAYER_12)
			.value("LAYER_13", CollisionLayer::LAYER_13)
			.value("LAYER_14", CollisionLayer::LAYER_14)
			.value("LAYER_15", CollisionLayer::LAYER_15)
			.value("LAYER_16", CollisionLayer::LAYER_16)
			.def(py::self | py::self)
			.def(py::self & py::self)
			.def(py::self ^ py::self)
			.def(~py::self);

		py::enum_<CollisionMask>(m, "CollisionMask",
			"Collision mask bit flags. Combine multiple with |, e.g. "
			"CollisionMask.LAYER_1 | CollisionMask.LAYER_3")
			.value("LAYER_1", CollisionMask::LAYER_1)
			.value("LAYER_2", CollisionMask::LAYER_2)
			.value("LAYER_3", CollisionMask::LAYER_3)
			.value("LAYER_4", CollisionMask::LAYER_4)
			.value("LAYER_5", CollisionMask::LAYER_5)
			.value("LAYER_6", CollisionMask::LAYER_6)
			.value("LAYER_7", CollisionMask::LAYER_7)
			.value("LAYER_8", CollisionMask::LAYER_8)
			.value("LAYER_9", CollisionMask::LAYER_9)
			.value("LAYER_10", CollisionMask::LAYER_10)
			.value("LAYER_11", CollisionMask::LAYER_11)
			.value("LAYER_12", CollisionMask::LAYER_12)
			.value("LAYER_13", CollisionMask::LAYER_13)
			.value("LAYER_14", CollisionMask::LAYER_14)
			.value("LAYER_15", CollisionMask::LAYER_15)
			.value("LAYER_16", CollisionMask::LAYER_16)
			.def(py::self | py::self)
			.def(py::self & py::self)
			.def(py::self ^ py::self)
			.def(~py::self);

		m.def("layer_overlap", [](uint16_t layerA, uint16_t maskA, uint16_t layerB, uint16_t maskB) {
			return layerOverlap(layerA, maskA, layerB, maskB);
			}, py::arg("layer_a"), py::arg("mask_a"), py::arg("layer_b"), py::arg("mask_b"),
				"Check whether an (layer, mask) pair on object A would collide with (layer, mask) on object B");

		py::enum_<CollisionType>(m, "CollisionType")
			.value("RigidVsRigid", CollisionType::RigidVsRigid)
			.value("RigidVsStatic", CollisionType::RigidVsStatic)
			.value("StaticVsStatic", CollisionType::StaticVsStatic)
			.value("RigidVsSoft", CollisionType::RigidVsSoft)
			.value("SoftVsSoft", CollisionType::SoftVsSoft)
			.value("FluidVsRigid", CollisionType::FluidVsRigid)
			.value("FluidVsSoft", CollisionType::FluidVsSoft)
			.export_values();

		py::class_<CollisionEventData>(m, "CollisionEventData")
			.def_readonly("type", &CollisionEventData::type)
			.def_property_readonly("self", [](CollisionEventData& self) -> Object* { return self.self; },
				py::return_value_policy::reference)
			.def_property_readonly("other", [](CollisionEventData& self) -> Object* { return self.other; },
				py::return_value_policy::reference)
			.def_readonly("point", &CollisionEventData::point)
			.def_readonly("normal", &CollisionEventData::normal)
			.def_readonly("penetration", &CollisionEventData::penetration)
			.def("__repr__", [](const CollisionEventData& d) {
			std::ostringstream ss;
			ss << "CollisionEventData(penetration=" << d.penetration << ")";
			return ss.str();
				});

		py::class_<RayCastHit>(m, "RayCastHit")
			.def(py::init<>())
			.def_readonly("hit", &RayCastHit::hit)
			.def_property_readonly("object", [](RayCastHit& self) -> Object* { return self.object; },
				py::return_value_policy::reference)
			.def_readonly("point", &RayCastHit::point)
			.def_readonly("normal", &RayCastHit::normal)
			.def_readonly("distance", &RayCastHit::distance)
			.def_readonly("edge_index", &RayCastHit::edgeIndex)
			.def_readonly("is_soft_body", &RayCastHit::isSoftBody)
			.def("__bool__", [](const RayCastHit& h) { return h.hit; },
				"Allows 'if hit:' instead of 'if hit.hit:'")
			.def("__repr__", [](const RayCastHit& h) {
			if (!h.hit) return std::string("RayCastHit(hit=False)");
			std::ostringstream ss;
			ss << "RayCastHit(hit=True, distance=" << h.distance << ")";
			return ss.str();
				});

		py::module_ physicsMod = m.def_submodule("Physics", "Physics engine queries");

		physicsMod.def("raycast", [](glm::vec3 origin, glm::vec3 direction, float length,
			std::optional<uint16_t> collisionLayer, std::vector<Object*> ignoreObjects) {
				return PhysicsEngine::getInstance().RayCast(origin, direction, length, collisionLayer, ignoreObjects);
			},
			py::arg("origin"), py::arg("direction"), py::arg("length"),
			py::arg("collision_layer") = py::none(), py::arg("ignore_objects") = std::vector<Object*>{},
			"Cast a ray and return the closest hit. collision_layer=None hits every layer. "
			"e.g. hit = Physics.raycast(pos, Vector3(0,-1,0), 5.0, CollisionMask.LAYER_1, [self.owner])");

		physicsMod.def("raycast_all", [](glm::vec3 origin, glm::vec3 direction, float length,
			std::optional<uint16_t> collisionLayer, std::vector<Object*> ignoreObjects) {
				return PhysicsEngine::getInstance().RayCastAll(origin, direction, length, collisionLayer, ignoreObjects);
			},
			py::arg("origin"), py::arg("direction"), py::arg("length"),
			py::arg("collision_layer") = py::none(), py::arg("ignore_objects") = std::vector<Object*>{},
			"Cast a ray and return every hit, sorted nearest-first.");
	}

	void RegisterComponentBindings(py::module_& m) {
		auto renderClass = py::class_<RenderComponent>(m, "RenderComponent")
			.def_property("enable",
				[](RenderComponent& self) { return self.Enabled; },
				[](RenderComponent& self, bool enable) { self.SetEnabled(enable); })

			.def_property("color",
				[](RenderComponent& self) { return self.color; },
				[](RenderComponent& self, glm::vec4 c) {
					self.color = c;
					EngineManager::getInstance().EngineChangeEvent();
				})

			.def_property("z_index",
				[](RenderComponent& self) { return self.z_index; },
				[](RenderComponent& self, int z) {
					self.z_index = z;
					EngineManager::getInstance().EngineChangeEvent();
				})

			.def_property("shape",
				[](RenderComponent& self) -> Shape { return self.currentShape; },
				[](RenderComponent& self, Shape shape) { self.SetShape(shape); })

			.def("set_shape", &RenderComponent::SetShape, py::arg("shape"),
				"Accepts a RectangleShape, CircleShape, or PolygonShape")

			.def("set_texture", [](RenderComponent& self, const std::string& virtualPath) {
			if (virtualPath.empty()) {
				self.SetTexture("");
				return;
			}
			if (!FileManager::getInstance().VirtualPathExists(virtualPath)) {
				Console::AddMessage(Console::MessageType::Error,
					"set_texture: resource not found: " + virtualPath);
				return;
			}
			self.SetTexture(FileManager::getInstance().VirtualToAbsolute(virtualPath).string());
				}, py::arg("virtual_path"),
					"Load a texture from a res:// path (e.g. 'res://textures/wood.png'), or pass '' to clear it")
			.def("set_enable", &RenderComponent::SetEnabled);

			
		EnableGetComponent<RenderComponent>(renderClass);
		EnableHasComponent<RenderComponent>(renderClass);
		RegisterComponentGetter<RenderComponent>(renderClass);
		EnableGetOwner<RenderComponent>(renderClass);          
		RegisterComponentRemover<RenderComponent>(renderClass); 
		RegisterComponentAdder<RenderComponent>(renderClass,
			[](Object& obj) {
				return std::make_unique<RenderComponent>(&obj, std::vector<float>{}, obj.shader, "");
			});
		EnableAddComponent<RenderComponent>(renderClass);    
		EnableRemoveComponent<RenderComponent>(renderClass);
		EnableAddObject<RenderComponent>(renderClass);   
		EnableAddChild<RenderComponent>(renderClass);
		EnableRemoveObject<RenderComponent>(renderClass);

		auto transformClass = py::class_<TransformComponent>(m, "TransformComponent")
			.def_property("world_position",
				[](TransformComponent& self) -> glm::vec3& { return self.worldPosition; },
				[](TransformComponent& self, glm::vec3 pos) { self.UpdateWorldPosition(pos); },
				py::return_value_policy::reference_internal)
			.def_property("enable", 
				[](TransformComponent& self) {return self.Enabled; },
				[](TransformComponent& self, bool enable) {self.SetEnabled(enable);})
			.def_property("rotation_degrees",
				[](TransformComponent& self) {return self.rotation * (180 / std::numbers::pi);},
				[](TransformComponent& self, float angle) {self.Rotate(angle * (std::numbers::pi / 180));})
			.def_property("rotation",
				[](TransformComponent& self) { return self.rotation; },
				[](TransformComponent& self, float angle) { self.Rotate(angle); })
			.def_property("size",
				[](TransformComponent& self) { return self.size; },
				[](TransformComponent& self, glm::vec3 scale) { self.Scale(scale); })
			.def("set_enable", &TransformComponent::SetEnabled)
			.def("set_size", &TransformComponent::Scale)
			.def("set_rotation", &TransformComponent::Rotate)
			.def("set_rotation_degrees", [](TransformComponent& self, float angle) { self.Rotate(angle * (180 / std::numbers::pi)); })
			.def("update_world_position", &TransformComponent::UpdateWorldPosition)
			.def("to_local_coordinates", [](TransformComponent& self, glm::vec3 worldCoordinates) {self.ProjectToWorld(worldCoordinates, true);})
			.def("to_world_coordinates", [](TransformComponent& self, glm::vec3 localCoordinates) {self.ProjectToWorld(localCoordinates, false);});
		EnableGetComponent<TransformComponent>(transformClass);
		EnableHasComponent<TransformComponent>(transformClass);
		RegisterComponentGetter<TransformComponent>(transformClass);
		EnableGetOwner<TransformComponent>(transformClass);         
		EnableAddComponent<TransformComponent>(transformClass);
		EnableRemoveComponent<TransformComponent>(transformClass);
		EnableAddObject<TransformComponent>(transformClass);   
		EnableAddChild<TransformComponent>(transformClass);
		EnableRemoveObject<TransformComponent>(transformClass);

		auto rigidBodyClass = py::class_<RigidBodyComponent>(m, "RigidBodyComponent")
			.def_property("enable",
				[](RigidBodyComponent& self) { return self.Enabled; },
				[](RigidBodyComponent& self, bool enable) { self.SetEnabled(enable); })
			.def("set_enable", &RigidBodyComponent::SetEnabled)

			.def_property("velocity",
				[](RigidBodyComponent& self) { return self.velocity; },
				[](RigidBodyComponent& self, glm::vec3 v) {
					self.velocity = v;
					EngineManager::getInstance().EngineChangeEvent();
				})
			.def("set_velocity", [](RigidBodyComponent& self, glm::vec3 v) {
			self.velocity = v;
			EngineManager::getInstance().EngineChangeEvent();
				}, py::arg("velocity"))

			.def_property("acceleration",
				[](RigidBodyComponent& self) { return self.netAcceleration; },
				[](RigidBodyComponent& self, glm::vec2 a) { self.netAcceleration = a; })
			.def("set_acceleration", [](RigidBodyComponent& self, glm::vec2 a) {
			self.netAcceleration = a;
				}, py::arg("acceleration"))

			.def_property("angular_velocity",
				[](RigidBodyComponent& self) { return self.angularVelocity; },
				[](RigidBodyComponent& self, float w) {
					self.angularVelocity = w;
					EngineManager::getInstance().EngineChangeEvent();
				})
			.def("set_angular_velocity", [](RigidBodyComponent& self, float w) {
			self.angularVelocity = w;
			EngineManager::getInstance().EngineChangeEvent();
				}, py::arg("angular_velocity"))

			.def_property("angular_acceleration",
				[](RigidBodyComponent& self) { return self.angularAcceleration; },
				[](RigidBodyComponent& self, float a) { self.angularAcceleration = a; })
			.def("set_angular_acceleration", [](RigidBodyComponent& self, float a) {
			self.angularAcceleration = a;
				}, py::arg("angular_acceleration"))

			.def_property("net_force",
				[](RigidBodyComponent& self) { return self.netForceDisplay; },
				[](RigidBodyComponent& self, glm::vec2 f) { self.netForceDisplay = f; })
			.def("set_net_force", [](RigidBodyComponent& self, glm::vec2 f) {
			self.netForceDisplay = f;
				}, py::arg("net_force"))

			.def_property("torque",
				[](RigidBodyComponent& self) { return self.torqueDisplay; },
				[](RigidBodyComponent& self, float t) { self.torqueDisplay = t; })
			.def("set_torque", [](RigidBodyComponent& self, float t) {
			self.torqueDisplay = t;
				}, py::arg("torque"))

			.def_property("mass",
				[](RigidBodyComponent& self) { return 1.0f / self.inverseMass; },
				[](RigidBodyComponent& self, float mass) {
					if (mass <= 0) mass = 0.001f;
					self.inverseMass = 1.0f / mass;
					EngineManager::getInstance().EngineChangeEvent();
					self.CalculateInertia();
				})
			.def("set_mass", [](RigidBodyComponent& self, float mass) {
			if (mass <= 0) mass = 0.001f;
			self.inverseMass = 1.0f / mass;
			EngineManager::getInstance().EngineChangeEvent();
			self.CalculateInertia();
				}, py::arg("mass"))

			.def_property("inverse_mass",
				[](RigidBodyComponent& self) { return self.inverseMass; },
				[](RigidBodyComponent& self, float invMass) {
					self.inverseMass = invMass;
					EngineManager::getInstance().EngineChangeEvent();
					self.CalculateInertia();
				})
			.def("set_inverse_mass", [](RigidBodyComponent& self, float invMass) {
			self.inverseMass = invMass;
			EngineManager::getInstance().EngineChangeEvent();
			self.CalculateInertia();
				}, py::arg("inverse_mass"))

			.def_property("inertia",
				[](RigidBodyComponent& self) { return self.Inertia; },
				[](RigidBodyComponent& self, float inertia) {
					self.Inertia = inertia;
					self.inverseInertia = inertia > 0 ? 1.0f / inertia : 0.0f;
				})
			.def("set_inertia", [](RigidBodyComponent& self, float inertia) {
			self.Inertia = inertia;
			self.inverseInertia = inertia > 0 ? 1.0f / inertia : 0.0f;
				}, py::arg("inertia"))

			.def_property("inverse_inertia",
				[](RigidBodyComponent& self) { return self.inverseInertia; },
				[](RigidBodyComponent& self, float invInertia) {
					self.inverseInertia = invInertia;
					self.Inertia = invInertia > 0 ? 1.0f / invInertia : 0.0f;
				})
			.def("set_inverse_inertia", [](RigidBodyComponent& self, float invInertia) {
			self.inverseInertia = invInertia;
			self.Inertia = invInertia > 0 ? 1.0f / invInertia : 0.0f;
				}, py::arg("inverse_inertia"))

			.def("recalculate_inertia", &RigidBodyComponent::CalculateInertia,
				"Recompute inertia from the current shape and mass, e.g. after resizing")

			.def_readwrite("linear_damping", &RigidBodyComponent::linearDamping)
			.def("set_linear_damping", [](RigidBodyComponent& self, float d) { self.linearDamping = d; },
				py::arg("linear_damping"))

			.def_readwrite("angular_damping", &RigidBodyComponent::angularDamping)
			.def("set_angular_damping", [](RigidBodyComponent& self, float d) { self.angularDamping = d; },
				py::arg("angular_damping"))

			.def("add_force", &RigidBodyComponent::AddForce, py::arg("force"),
				"Apply a force through the center of mass")
			.def("add_force_at_world_point", &RigidBodyComponent::AddForceAtPoint,
				py::arg("force"), py::arg("point"),
				"Apply a force at a point given in world coordinates")
			.def("add_force_at_local_point", &RigidBodyComponent::AddForceAtBodyPoint,
				py::arg("force"), py::arg("point"),
				"Apply a force at a point given in this object's local/model coordinates");

		EnableGetComponent<RigidBodyComponent>(rigidBodyClass);
		EnableHasComponent<RigidBodyComponent>(rigidBodyClass);
		RegisterComponentGetter<RigidBodyComponent>(rigidBodyClass);
		EnableGetOwner<RigidBodyComponent>(rigidBodyClass);
		RegisterComponentRemover<RigidBodyComponent>(rigidBodyClass);
		RegisterComponentAdder<RigidBodyComponent>(rigidBodyClass,
			[](Object& obj) {
				return std::make_unique<RigidBodyComponent>(&obj);
			});
		EnableAddComponent<RigidBodyComponent>(rigidBodyClass);
		EnableRemoveComponent<RigidBodyComponent>(rigidBodyClass);
		EnableAddObject<RigidBodyComponent>(rigidBodyClass);
		EnableAddChild<RigidBodyComponent>(rigidBodyClass);
		EnableRemoveObject<RigidBodyComponent>(rigidBodyClass);

		auto collisionClass = py::class_<CollisionComponent>(m, "CollisionComponent")
			.def_property("enable",
				[](CollisionComponent& self) { return self.Enabled; },
				[](CollisionComponent& self, bool enable) { self.SetEnabled(enable); })
			.def("set_enable", &CollisionComponent::SetEnabled)

			.def_property("is_static",
				[](CollisionComponent& self) { return self.isStatic; },
				[](CollisionComponent& self, bool s) { self.isStatic = s; })
			.def("set_static", [](CollisionComponent& self, bool s) { self.isStatic = s; },
				py::arg("is_static"))

			.def_property("collision_layer",
				[](CollisionComponent& self) { return self.collisionLayer; },
				[](CollisionComponent& self, uint16_t layer) { self.SetCollisionLayer(layer); })
			.def("set_collision_layer", &CollisionComponent::SetCollisionLayer, py::arg("layer"))

			.def_property("collision_mask",
				[](CollisionComponent& self) { return self.collisionMask; },
				[](CollisionComponent& self, uint16_t mask) { self.SetCollisionMask(mask); })
			.def("set_collision_mask", &CollisionComponent::SetCollisionMask, py::arg("mask"))

			.def_property("sync_with_render_component",
				[](CollisionComponent& self) { return self.syncWithRenderComponent; },
				[](CollisionComponent& self, bool sync) { self.SetSyncWithRenderComponent(sync); })
			.def("set_sync_with_render_component", &CollisionComponent::SetSyncWithRenderComponent,
				py::arg("sync"))

			.def_property("shape",
				[](CollisionComponent& self) -> Shape { return self.currentShape; },
				[](CollisionComponent& self, Shape shape) { self.SetShape(shape); })
			.def("set_shape", [](CollisionComponent& self, Shape shape) {
			if (self.syncWithRenderComponent) {
				Console::PrintWarning("CollisionComponent: set_shape called while sync_with_render_component is True; shape will be overridden by RenderComponent");
			}
			self.SetShape(shape);
				}, py::arg("shape"),
					"Accepts a RectangleShape, CircleShape, or PolygonShape")

			.def("is_grounded", &CollisionComponent::isGrounded, py::arg("probe_length") = 0.15f,
				"Cast a short ray straight down from the lowest point of this shape to check for ground")

			.def("add_collision_callback", [](CollisionComponent& self, py::function func) {
			return self.AddCollisionCallback([func](const CollisionEventData& data) {
				py::gil_scoped_acquire gil;
				try { func(data); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("collision callback error: ") + e.what());
				}
				});
				}, py::arg("callback"),
					"Fires every physics substep while two shapes are overlapping (stay-style). Returns an id usable with remove_collision_callback().")
			.def("remove_collision_callback", &CollisionComponent::RemoveCollisionCallback, py::arg("id"))

			.def("add_collision_enter_callback", [](CollisionComponent& self, py::function func) {
			return self.AddCollisionEnterCallback([func](const CollisionEventData& data) {
				py::gil_scoped_acquire gil;
				try { func(data); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("collision enter callback error: ") + e.what());
				}
				});
				}, py::arg("callback"),
					"Fires once on the frame a collision first begins.")
			.def("remove_collision_enter_callback", &CollisionComponent::RemoveCollisionEnterCallback, py::arg("id"))

			.def("add_collision_exit_callback", [](CollisionComponent& self, py::function func) {
			return self.AddCollisionExitCallback([func](const CollisionEventData& data) {
				py::gil_scoped_acquire gil;
				try { func(data); }
				catch (const py::error_already_set& e) {
					Console::AddMessage(Console::MessageType::Error,
						std::string("collision exit callback error: ") + e.what());
				}
				});
				}, py::arg("callback"),
					"Fires once on the frame a collision stops.")
			.def("remove_collision_exit_callback", &CollisionComponent::RemoveCollisionExitCallback, py::arg("id"));

		EnableGetComponent<CollisionComponent>(collisionClass);
		EnableHasComponent<CollisionComponent>(collisionClass);
		RegisterComponentGetter<CollisionComponent>(collisionClass);
		EnableGetOwner<CollisionComponent>(collisionClass);
		RegisterComponentRemover<CollisionComponent>(collisionClass);
		RegisterComponentAdder<CollisionComponent>(collisionClass,
			[](Object& obj) {
				return std::make_unique<CollisionComponent>(&obj);
			});
		EnableAddComponent<CollisionComponent>(collisionClass);
		EnableRemoveComponent<CollisionComponent>(collisionClass);
		EnableAddObject<CollisionComponent>(collisionClass);
		EnableAddChild<CollisionComponent>(collisionClass);
		EnableRemoveObject<CollisionComponent>(collisionClass);
	}

	Object* CreateDefaultObject() {
		Object* obj = new Object(Shader("Resources/Shaders/vertex.txt", "Resources/Shaders/fragment.txt"));

		obj->AddComponent(std::make_unique<EditorRenderComponent>(obj, obj->shader, "Resources/Images/Object.png", 0.075f));
		obj->AddComponent(std::make_unique<TransformComponent>(obj, obj->shader,
			obj->GetComponent<EditorRenderComponent>()->GetCenter()));
		obj->AddComponent(std::make_unique<MouseInteractComponent>(obj, false));

		return obj;
	}

	void RegisterObjectBindings(py::module_& m) {
		py::class_<Object, std::unique_ptr<Object, py::nodelete>>(m, "Object")
			.def(py::init(&CreateDefaultObject),
				"Creates a new Object, not yet part of the scene — call "
				"add_object() to insert it.")
			.def_readwrite("name", &Object::name)
			.def_readwrite("hidden", &Object::hidden)
			.def_property_readonly("id", [](Object& self) { return self.id; })
			.def_property_readonly("parent", [](Object& self) -> Object* {
			return self.parent;
				}, py::return_value_policy::reference)
			.def_property_readonly("parent_id", [](Object& self) { return self.parentID; })
			.def_property_readonly("children", [](Object& self) {
			return self.children;
				}, py::return_value_policy::reference)

			.def("set_name", [](Object& self, const std::string& name) { self.name = name; },
				py::arg("name"))
			.def("show", &Object::Show)
			.def("hide", &Object::Hide)
			.def("get_parent", [](Object& self) -> Object* {
			return self.parent;
				}, py::return_value_policy::reference)
			.def("get_parent_id", [](Object& self) { return self.parentID; })
			.def("get_children", [](Object& self) {
			return self.children;
				}, py::return_value_policy::reference)
			.def("get_children_count", [](Object& self) { return self.children.size(); })
			.def("get_component", [](Object& self, py::object componentClass) {
			return GetComponentByPythonType(&self, componentClass);
				}, py::arg("component_class"),
					"Look up a component on this Object")
			.def("add_component", &AddComponentToObject, py::arg("component"),
				"Attach a component instance to this Object, e.g. obj.add_component(RenderComponent)")
			.def("remove_component", &RemoveComponentFromObject, py::arg("component_class"),
				"Remove a component of the given type from this Object, e.g. obj.remove_component(RenderComponent)")
			.def("add_object", [](Object&, Object* obj, Object* parent) {
			return AddObjectToScene(obj, parent);
				}, py::arg("obj"), py::arg("parent") = nullptr,
					py::return_value_policy::reference,
					"Add a newly created Object to the scene, optionally parented to another Object")
			.def("add_child", [](Object& self, Object* obj) {
			return AddObjectToScene(obj, &self);
				}, py::arg("obj"),
					py::return_value_policy::reference,
					"Add obj as a child of this Object")
			.def("remove_object", [](Object&, Object* obj) {
			RemoveObjectFromScene(obj);
				}, py::arg("obj"),
					"Remove an object from the scene");

	}

	void ResetDynamicComponentRegistriesImpl() {
		ComponentRegistry().clear();
		ComponentRemoverRegistry().clear();
		ComponentAdderRegistry().clear();
	}
}

std::string VirtualPathToModuleName(const std::string& virtualPath) {
	std::string path = virtualPath;
	const std::string prefix = "res://";
	if (path.rfind(prefix, 0) == 0) path = path.substr(prefix.size());
	if (path.size() >= 3 && path.substr(path.size() - 3) == ".py") {
		path = path.substr(0, path.size() - 3);
	}
	std::replace(path.begin(), path.end(), '/', '.');
	std::replace(path.begin(), path.end(), '\\', '.');
	return path;
}

py::object ImportScriptClass(const std::string& virtualSourcePath) {
	std::string moduleName = VirtualPathToModuleName(virtualSourcePath);
	std::filesystem::path absPath = FileManager::getInstance().VirtualToAbsolute(virtualSourcePath);

	py::gil_scoped_acquire gil;

	py::object sysModule = py::module_::import("sys");
	py::dict sysModules = sysModule.attr("modules");

	py::object moduleObj;

	if (sysModules.contains(moduleName)) {
		moduleObj = sysModules[moduleName.c_str()];
	}
	else {
		py::object importlibUtil = py::module_::import("importlib.util");

		py::object spec = importlibUtil.attr("spec_from_file_location")(moduleName, absPath.string());
		if (spec.is_none()) {
			throw py::value_error("Failed to load script '" + virtualSourcePath +
				"': could not create a module spec for '" + absPath.string() + "'");
		}

		moduleObj = importlibUtil.attr("module_from_spec")(spec);

		sysModules[moduleName.c_str()] = moduleObj;

		try {
			spec.attr("loader").attr("exec_module")(moduleObj);
		}
		catch (const py::error_already_set&) {
			sysModules.attr("pop")(moduleName, py::none());
			throw;
		}
	}

	py::object fusionModule = py::module_::import("fusion");
	py::object scriptBaseClass = fusionModule.attr("Script");

	py::object foundClass;
	py::dict moduleDict = moduleObj.attr("__dict__");
	for (auto item : moduleDict) {
		py::object value = py::reinterpret_borrow<py::object>(item.second);
		if (!PyType_Check(value.ptr())) continue;
		if (value.is(scriptBaseClass)) continue;

		int isSub = PyObject_IsSubclass(value.ptr(), scriptBaseClass.ptr());
		if (isSub != 1) { if (isSub < 0) PyErr_Clear(); continue; }

		if (py::str(value.attr("__module__")).cast<std::string>() == moduleName) {
			foundClass = value;
			break;
		}
	}

	if (!foundClass) {
		throw py::value_error("Failed to load script '" + virtualSourcePath +
			"': no class inheriting from Script found in module '" + moduleName + "'");
	}

	RegisterScriptClassAsComponentType(foundClass, virtualSourcePath);
	return foundClass;
}

void ResetDynamicComponentRegistries() {
	ResetDynamicComponentRegistriesImpl();
}

void RegisterEngineBindings(py::module_& m) {
	m.doc() = "Fusion engine scripting API";
	RegisterMathBindings(m);
	RegisterPhysicsBindings(m);
	RegisterShapeBindings(m);
	RegisterScriptBindings(m);
	RegisterInputBindings(m);
	RegisterConsoleBindings(m);
	RegisterComponentBindings(m);
	RegisterObjectBindings(m);
}