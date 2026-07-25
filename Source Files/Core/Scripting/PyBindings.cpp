#include "../../../Header Files/Core/Scripting/PyBindings.h"
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

			.def(py::self + py::self)
			.def(py::self - py::self)
			.def(py::self * py::self)
			.def(py::self * float())
			.def(float() * py::self)
			.def(py::self / float())
			.def(-py::self)
			.def(py::self == py::self)
			.def(py::self != py::self)

			.def("length", [](const glm::vec2& v) { return glm::length(v) ;})
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
}

void RegisterEngineBindings(py::module_& m) {
	m.doc() = "Fusion engine scripting API";
	RegisterMathBindings(m);
}