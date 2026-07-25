#include "../../../Header Files/Core/Scripting/PyBindings.h"

PYBIND11_MODULE(fusion, m) {
	RegisterEngineBindings(m);
}