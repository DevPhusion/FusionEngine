#pragma once
#include <pybind11/pybind11.h>

class Object;

class ScriptBase {
public:
	virtual ~ScriptBase() = default;
	Object* parent = nullptr; 
};

struct ExportMarker {
	pybind11::object value;
};

void RegisterEngineBindings(pybind11::module_& m);