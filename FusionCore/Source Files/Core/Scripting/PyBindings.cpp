#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/Editor/Windows/Console.h"
#include "../../../Header Files/Components/TransformComponent.h"
#include <pybind11/stl.h>
#include <pybind11/operators.h>   
#include <glm/glm.hpp>
#include <sstream>

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
	}

	std::unordered_map<PyObject*, std::function<py::object(Object*)>>& ComponentRegistry() {
		static std::unordered_map<PyObject*, std::function<py::object(Object*)>> registry;
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

	template <typename T, typename PyClass>
	void EnableGetComponent(PyClass& cls) {
		cls.def("get_component", [](T& self, py::object componentClass) {
			return GetComponentByPythonType(self.parent, componentClass);
			}, py::arg("component_class"),
				"Look up a component on this Object");
	}

	template <typename T>
	void RegisterComponentGetter(py::object pyClass) {
		ComponentRegistry()[pyClass.ptr()] = [](Object* obj) -> py::object {
			T* comp = obj->GetComponent<T>();
			if (!comp) return py::none();
			return py::cast(comp, py::return_value_policy::reference);
			};
	}

	void RegisterScriptBindings(py::module_& m) {
		auto scriptClass = py::class_<ScriptBase, std::shared_ptr<ScriptBase>>(m, "Script")
			.def(py::init<>());

		EnableGetComponent<ScriptBase>(scriptClass);

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

	void RegisterComponentBindings(py::module_& m) {
		auto transformClass = py::class_<TransformComponent>(m, "TransformComponent")
			.def_property("world_position", &TransformComponent::GetWorldPosition, &TransformComponent::UpdateWorldPosition)
			.def_property("rotation",
				[](TransformComponent& self) { return self.rotation; },
				[](TransformComponent& self, float angle) { self.Rotate(angle); })
			.def_property("size",
				[](TransformComponent& self) { return self.size; },
				[](TransformComponent& self, glm::vec3 scale) { self.Scale(scale); })
			.def("set_size", &TransformComponent::Scale)
			.def("set_rotation", &TransformComponent::Rotate)
			.def("update_world_position", &TransformComponent::UpdateWorldPosition);

		EnableGetComponent<TransformComponent>(transformClass);
		RegisterComponentGetter<TransformComponent>(transformClass);
	}
}

void RegisterEngineBindings(py::module_& m) {
	m.doc() = "Fusion engine scripting API";
	RegisterMathBindings(m);
	RegisterScriptBindings(m);
	RegisterConsoleBindings(m);
	RegisterComponentBindings(m);
}