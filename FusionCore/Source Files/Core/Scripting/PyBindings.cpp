#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/Editor/Windows/Console.h"
#include "../../../Header Files/Components/TransformComponent.h"
#include <pybind11/stl.h>
#include <pybind11/operators.h>  
#include <glm/glm.hpp>
#include <sstream>
#include <numbers>

namespace py = pybind11;

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

	py::object GetComponentByPythonType(Object* parent, const py::object& componentClass) {
		if (!parent) return py::none();

		auto& registry = ComponentRegistry();
		auto it = registry.find(componentClass.ptr());
		if (it == registry.end()) {
			throw py::type_error(
				"get_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}
		return it->second(parent);
	}

	void RemoveComponentFromObject(Object& obj, py::object componentClass) {
		auto& registry = ComponentRemoverRegistry();
		auto it = registry.find(componentClass.ptr());
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
		auto it = registry.find(componentClass.ptr());
		if (it == registry.end()) {
			throw py::type_error(
				"has_component: '" + py::str(componentClass).cast<std::string>() +
				"' is not a registered engine component type");
		}
		return !it->second(obj).is_none();
	}

	py::object AddComponentToObject(Object& obj, py::object componentClass) {
		auto& registry = ComponentAdderRegistry();
		auto it = registry.find(componentClass.ptr());
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

		py::class_<ExportMarker>(m, "_ExportMarker")
			.def(py::init<py::object>());

		m.def("export", [](py::object value) {
			return ExportMarker{ value };
			}, py::arg("value"),
				"Mark a script attribute as editable in the inspector");
	}

	void RegisterConsoleBindings(py::module_& m) {
		py::class_<Console>(m, "Console")
			.def_static("Print", [](const std::string& text) {
			Console::AddMessage(Console::MessageType::Info, text);
				}, py::arg("text"))
			.def_static("PrintWarning", [](const std::string& text) {
			Console::AddMessage(Console::MessageType::Warning, text);
				}, py::arg("text"))
			.def_static("PrintError", [](const std::string& text) {
			Console::AddMessage(Console::MessageType::Error, text);
				}, py::arg("text"));
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
	}

	void RegisterObjectBindings(py::module_& m) {
		py::class_<Object, std::unique_ptr<Object, py::nodelete>>(m, "Object")
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
				"Remove a component of the given type from this Object, e.g. obj.remove_component(RenderComponent)");
	}

}

void RegisterEngineBindings(py::module_& m) {
	m.doc() = "Fusion engine scripting API";
	RegisterMathBindings(m);
	RegisterShapeBindings(m);
	RegisterScriptBindings(m);
	RegisterInputBindings(m);
	RegisterConsoleBindings(m);
	RegisterComponentBindings(m);
	RegisterObjectBindings(m);
}