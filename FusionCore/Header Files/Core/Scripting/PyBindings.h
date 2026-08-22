#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "ExportedProperty.h"

namespace py = pybind11;

class Object;

class ScriptBase {
public:
	virtual ~ScriptBase() = default;
	Object* parent = nullptr; 
};

void RegisterEngineBindings(pybind11::module_& m);
void ResetDynamicComponentRegistries();
py::object ImportScriptClass(const std::string& virtualSourcePath);
std::string VirtualPathToModuleName(const std::string& virtualPath);